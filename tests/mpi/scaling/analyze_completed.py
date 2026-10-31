#!/usr/bin/env python3
"""Validate completed physical outputs, then compute qualified scaling ratios."""
from pathlib import Path
import csv,json,subprocess,sys,statistics
root=Path(sys.argv[1]).resolve(); here=Path(__file__).resolve().parent
subprocess.run([sys.executable,str(here/'summarize.py'),str(root)],check=True)
summary=json.loads((root/'summary.json').read_text()); checks={}; comparisons=root/'comparisons'; comparisons.mkdir(exist_ok=True)
for s in summary:
    if s['case']=='kernels' or s['ranks']==0 or not s['completed']: continue
    original=next((b for b in summary if b['case']==s['case'] and b['emax']==s['emax'] and b['ranks']==0 and b['completed']),None)
    if original is None: continue
    dest=comparisons/s['run']; dest.mkdir(exist_ok=True)
    prefix=f"{s['case']}_e{s['emax']}"
    for name,run in [(prefix+'_original',original['run']),(prefix+f"_mpi{s['ranks']}",s['run'])]:
        link=dest/name
        if not link.exists(): link.symlink_to(root/'runs'/run,target_is_directory=True)
    with (dest/'comparison.txt').open('w') as out:
        result=subprocess.run([sys.executable,str(here.parent/'compare.py'),str(dest),str(s['ranks'])],stdout=out,stderr=subprocess.STDOUT)
    checks[s['run']]=result.returncode==0
rows=[]
for s in summary:
    if s['case']=='kernels' or not s['completed'] or s['ranks']==0: continue
    converged=bool(s['convergence']) and all(x['eta_below_criterion'] for x in s['convergence'])
    for b in summary:
        if b['case']!=s['case'] or b['emax']!=s['emax'] or b['ranks'] not in (0,1) or not b['completed']: continue
        if not s['wall_seconds'] or not b['wall_seconds']: continue
        row={'run':s['run'],'baseline':b['run'],'converged':converged,'original_output_comparison':checks.get(s['run'],'unavailable'),'raw_wall_ratio':b['wall_seconds']/s['wall_seconds'],'allocated_core_seconds_ratio':(max(s['ranks'],1)*s['threads']*s['wall_seconds'])/(max(b['ranks'],1)*b['threads']*b['wall_seconds'])}
        baseline_converged=bool(b['convergence']) and all(x['eta_below_criterion'] for x in b['convergence'])
        row['accepted_speedup']=row['raw_wall_ratio'] if converged and baseline_converged and checks.get(s['run']) else ''
        rows.append(row)
with (root/'scaling.tsv').open('w') as out:
    columns=['run','baseline','converged','original_output_comparison','raw_wall_ratio','allocated_core_seconds_ratio','accepted_speedup']
    writer=csv.DictWriter(out,columns,delimiter='\t'); writer.writeheader(); writer.writerows(rows)
(root/'validation.json').write_text(json.dumps(checks,indent=2)+'\n')
# Preserve scheduler-reported CPU/RSS/exit status separately from wall timings.
if (root/'jobs.json').exists():
    ids=','.join(j['jobid'] for j in json.loads((root/'jobs.json').read_text()))
    with (root/'accounting.txt').open('w') as out:
        subprocess.run(['sacct','-j',ids,'--format=JobID,State,Elapsed,AllocCPUS,TotalCPU,MaxRSS,ExitCode','--parsable2'],stdout=out,check=True)

with (root/'kernel-scaling.tsv').open('w') as out:
    columns=['run','J','samples','min_seconds','median_seconds','max_seconds','speedup_vs_one_rank']
    writer=csv.DictWriter(out,columns,delimiter='\t'); writer.writeheader()
    for s in summary:
        if s['case']!='kernels' or not s['completed']: continue
        for j in (0,1,2):
            values=[v['seconds'] for v in s['kernel_samples'] if v['J']==j]
            if not values: continue
            base=next((b for b in summary if b['case']=='kernels' and b['emax']==s['emax'] and b['ranks']==1 and b['completed']),None)
            bv=[v['seconds'] for v in base['kernel_samples'] if v['J']==j] if base else []
            writer.writerow(dict(run=s['run'],J=j,samples=len(values),min_seconds=min(values),median_seconds=statistics.median(values),max_seconds=max(values),speedup_vs_one_rank=statistics.median(bv)/statistics.median(values) if bv else ''))
# Phase costs are disjoint checkpoints; internal function timers are inclusive.
phase_rows=list(csv.DictReader((root/'phases.tsv').open(),delimiter='\t'))
index={(x['run'],x['sequence'],x['phase'],x['metric']):x for x in phase_rows}
with (root/'phase-scaling.tsv').open('w') as out:
    columns=['run','phase','ranks_observed','phase_max_seconds','phase_min_seconds','phase_mean_seconds','broadcast_mean_seconds_including_wait','broadcast_rank_spread_seconds','pack_copy_mean_seconds','speedup_vs_one_rank_phase']
    writer=csv.DictWriter(out,columns,delimiter='\t'); writer.writeheader()
    for x in phase_rows:
        if x['metric']!='phase_seconds': continue
        s=next(v for v in summary if v['run']==x['run'])
        base=next((b for b in summary if b['case']==s['case'] and b['emax']==s['emax'] and b['ranks']==1 and b['threads']==s['threads']),None)
        b=index.get((base['run'],x['sequence'],x['phase'],'phase_seconds')) if base else None
        comm=index.get((x['run'],x['sequence'],x['phase'],'metric:broadcast_seconds'),{})
        pack=index.get((x['run'],x['sequence'],x['phase'],'metric:block_pack_copy_seconds'),{})
        writer.writerow(dict(run=x['run'],phase=x['phase'],ranks_observed=x['ranks_observed'],phase_max_seconds=x['max'],phase_min_seconds=x['min'],phase_mean_seconds=x['mean'],broadcast_mean_seconds_including_wait=comm.get('mean',0),broadcast_rank_spread_seconds=float(comm.get('max',0))-float(comm.get('min',0)),pack_copy_mean_seconds=pack.get('mean',0),speedup_vs_one_rank_phase=float(b['max'])/float(x['max']) if b and float(x['max'])>0 else ''))

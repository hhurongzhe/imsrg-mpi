#!/usr/bin/env python3
"""Aggregate completed or partial profiles without mistaking partial runs for speedups."""
import csv,json,re,statistics,sys
from pathlib import Path
root=Path(sys.argv[1]); rows=[]; summaries=[]
for run in sorted((root/'runs').glob('*')):
    if not run.is_dir(): continue
    match=re.fullmatch(r'(.+)_e(\d+)_r(\d+)_t(\d+)_n(\d+)',run.name)
    if not match: continue
    case,e,r,t,n=match.groups(); ranks=int(r) or 1
    groups={}; last_per_rank={}
    for path in sorted((run/'profile').glob('rank-*.tsv')):
        previous={}; phases={}
        for line in path.read_text().splitlines():
            try: seq,label,key,value=line.split('\t'); value=float(value)
            except ValueError: continue # A running process may be appending a record.
            phases.setdefault((int(seq),label),{})[key]=value
        for (seq,label),values in sorted(phases.items()):
            for key,value in values.items():
                delta=value if key=='phase_seconds' else value-previous.get(key,0.)
                groups.setdefault((seq,label,key),[]).append(delta)
            previous.update(values)
        last_per_rank[path.name]=max((seq for seq,_ in phases),default=-1)
    for (seq,label,key),values in sorted(groups.items()):
        rows.append({'run':run.name,'sequence':seq,'phase':label,'metric':key,'ranks_observed':len(values),'ranks_expected':ranks,'min':min(values),'mean':statistics.mean(values),'max':max(values)})
    status={}
    if (run/'status.txt').exists(): status=dict(line.split('=',1) for line in (run/'status.txt').read_text().splitlines() if '=' in line)
    log=(run/'console.log').read_text(errors='replace').replace('\x00','') if (run/'console.log').exists() else ''
    wall=re.findall(r'^real ([\d.]+)$',log,re.M)
    completed=(run/'COMPLETED').exists() and status.get('exit_code')=='0'
    convergence=None
    if (run/'flow.dat').exists():
        segments=[]; current=[]; last_step=-1
        for line in (run/'flow.dat').read_text().splitlines():
            try: values=list(map(float,line.split()[:13]))
            except ValueError: continue
            if len(values)!=13: continue
            if values[0]<=last_step and current: segments.append(current); current=[]
            current.append(values); last_step=values[0]
        if current: segments.append(current)
        # Columns 8/9 contain one-/two-body eta norms. Both must reach criterion.
        convergence=[{'rows':len(seg),'last_s':seg[-1][1],'eta1':seg[-1][8],'eta2':seg[-1][9], 'eta_below_criterion':(seg[-1][8]**2+seg[-1][9]**2)**.5 <=1e-6} for seg in segments]
    summaries.append({'run':run.name,'case':case,'emax':int(e),'ranks':int(r),'threads':int(t),'nodes':int(n),'completed':completed,'status':status,'wall_seconds':float(wall[-1]) if completed and wall else None,'convergence':convergence,'last_profile_sequence':last_per_rank,'kernel_samples':[{'J':int(j),'repeat':int(rep),'seconds':float(sec)} for j,rep,sec in re.findall(r'KERNEL J=(\d+) repeat=(\d+) seconds=([\d.eE+-]+)',log)]})
(root/'summary.json').write_text(json.dumps(summaries,indent=2)+'\n')
with (root/'phases.tsv').open('w') as f:
    columns=['run','sequence','phase','metric','ranks_observed','ranks_expected','min','mean','max']
    writer=csv.DictWriter(f,columns,delimiter='\t'); writer.writeheader(); writer.writerows(rows)
for s in summaries:
    print(s['run'], 'COMPLETED' if s['completed'] else 'INCOMPLETE',s['wall_seconds'])

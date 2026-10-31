#!/usr/bin/env python3
import csv,json,math,re,statistics,sys
from pathlib import Path
root=Path(sys.argv[1]); rows=[]
for run in sorted(root.glob('paired-*')):
 if not run.is_dir() or not (run/'console.log').exists(): continue
 log=(run/'console.log').read_text(errors='replace').replace('\0','')
 samples=re.findall(r'PAIR J=(\d+) mode=(full|compact) repeat=(\d+) seconds=([^ ]+) max_error=([^\n]+)',log)
 complete=(run/'COMPLETED').exists() and 'PAIRED_CHECK PASS' in log
 for j in (0,1,2):
  values={m:[float(t) for jj,mode,r,t,e in samples if int(jj)==j and mode==m] for m in ('full','compact')}
  errors=[float(e) for jj,m,r,t,e in samples if int(jj)==j]
  if not errors: continue
  valid=complete and all(len(v)==3 for v in values.values()) and all(math.isfinite(v) for vs in values.values() for v in vs) and all(math.isfinite(e) for e in errors)
  old=statistics.median(values['full']) if values['full'] else None
  new=statistics.median(values['compact']) if values['compact'] else None
  rows.append(dict(run=run.name,J=j,accepted=valid,full_seconds=old,compact_seconds=new,speedup=old/new if valid else '',max_error=max(errors)))
with (root/'paired-summary.tsv').open('w') as out:
 columns=['run','J','accepted','full_seconds','compact_seconds','speedup','max_error']
 writer=csv.DictWriter(out,columns,delimiter='\t');writer.writeheader();writer.writerows(rows)
for row in rows: print(json.dumps(row))

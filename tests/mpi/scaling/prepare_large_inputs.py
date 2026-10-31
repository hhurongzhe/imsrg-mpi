#!/usr/bin/env python3
"""Lossless, streaming adaptation of the large NN and full 3N fixtures."""
import array, hashlib, json, os, shutil, sys
from pathlib import Path
out=Path(sys.argv[1]); out.mkdir(parents=True,exist_ok=True)
nn=Path('/home/hrz/scratch/2BME/TwBME_N3LO_EM500_srg1.8_hw16_emax12_e2max24.me2j.bin')
three=Path('/home/hrz/scratch/3BME/ThBME_full_EM1.8_2.0_hw16_emax14_e2max28_e3max16.stream.bin')
manifest=out/'inputs.json'
if manifest.exists():
    data=json.loads(manifest.read_text())
    for key in ('nn','three'):
        assert Path(data[key]['output']).stat().st_size==data[key]['output_bytes']
    print('Reusing completed inputs:',manifest); raise SystemExit
records={}
for key,src,name in [('nn',nn,'EM18_hw16_e12.me2j'),('three',three,'EM18_hw16_e14_E3_16.bin')]:
    target=out/name; partial=target.with_suffix(target.suffix+'.partial'); digest=hashlib.sha256()
    count=0
    with src.open('rb') as inp, partial.open('wb') as dst:
        if key=='three': dst.write(b'NuHamil full 3N float32 stream; unchanged payload'.ljust(255,b' '))
        else: dst.write(b'# NuHamil NN float64 stream, lossless ASCII adaptation\n')
        while raw:=inp.read(8*1024*1024):
            digest.update(raw); count+=len(raw)
            if key=='three': dst.write(raw)
            else:
                values=array.array('d'); values.frombytes(raw)
                if sys.byteorder!='little': values.byteswap()
                lines=[' '.join(format(v,'.17g') for v in values[i:i+4]) for i in range(0,len(values),4)]
                text='\n'.join(lines)+'\n'
                # Verify every double survives the textual conversion exactly.
                recovered=array.array('d',map(float,text.split()))
                assert recovered.tobytes()==values.tobytes()
                dst.write(text.encode())
    partial.replace(target)
    records[key]={'source':str(src),'source_bytes':count,'source_sha256':digest.hexdigest(),'output':str(target.resolve()),'output_bytes':target.stat().st_size}
    print(key,records[key],flush=True)
manifest.write_text(json.dumps(records,indent=2)+'\n')

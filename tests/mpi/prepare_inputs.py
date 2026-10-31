#!/usr/bin/env python3
"""Adapt NuHamil stream files to the unmodified IMSRG reader, without data loss.

NuHamil's NN .me2j.bin is headerless float64, whereas IMSRG's .bin reader
expects a 255-byte header and float32. Write NN as round-trip-safe ASCII.
The full 3N stream is float32: prepend the missing 255-byte header only.
The source files are opened read-only; outputs stay in this repository.
"""
import array
from pathlib import Path
import shutil
import sys

out = Path(__file__).resolve().parents[2] / 'temp/mpi_bench/inputs'
out.mkdir(parents=True, exist_ok=True)
nn = Path('/home/hrz/scratch/2BME/TwBME_N3LO_EM500_srg1.8_hw16_emax6_e2max12.me2j.bin')
nnn = Path('/home/hrz/scratch/3BME/ThBME_full_EM1.8_2.0_hw16_emax6_e2max12_e3max6.stream.bin')
target = out / 'EM18_hw16_e6.me2j'
values = array.array('d')
values.frombytes(nn.read_bytes())
if sys.byteorder != 'little': values.byteswap()
with target.open('w') as f:
    f.write('# NuHamil NN float64 stream, lossless ASCII adaptation\n')
    for i in range(0, len(values), 4):
        f.write(' '.join(format(v,'.17g') for v in values[i:i+4])+'\n')
# Verify exact float64 round trip.
with target.open() as f:
    next(f)
    recovered = array.array('d', (float(v) for line in f for v in line.split()))
assert recovered == values
with nnn.open('rb') as src, (out/'EM18_hw16_e6_E3_6.bin').open('wb') as dst:
    dst.write(b'NuHamil full 3N float32 stream; unchanged payload'.ljust(255,b' '))
    shutil.copyfileobj(src, dst)
assert (out/'EM18_hw16_e6_E3_6.bin').stat().st_size == nnn.stat().st_size + 255
print('NN values:',len(values),'3N values:',nnn.stat().st_size//4,'output:',out)

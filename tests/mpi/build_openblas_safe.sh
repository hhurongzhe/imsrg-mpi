#!/bin/bash
# Same local OpenBLAS source and kernels, but safe for concurrent OpenMP callers.
set -euo pipefail
cd /scratch/hrz/imsrg-mpi
source tests/mpi/env.sh
root=$PWD
if [[ ! -f temp/mpi_bench/openblas-safe-src/.extracted ]]; then
  python3 - <<'EXTRACT'
from pathlib import Path
from zipfile import ZipFile
out=Path('temp/mpi_bench/openblas-safe-src')
out.mkdir(parents=True,exist_ok=True)
with ZipFile('/home/hrz/opt_setup/openblas.zip') as z:
    for name in z.namelist():
        p=Path(name)
        if p.is_absolute() or '..' in p.parts: raise ValueError(name)
        if p.parts[0] == 'openblas':
            dest=Path(z.extract(name,out))
            mode=(z.getinfo(name).external_attr>>16)&0o777
            if dest.is_file() and mode: dest.chmod(mode)
(out/'.extracted').write_text('complete\n')
EXTRACT
fi
src=$root/temp/mpi_bench/openblas-safe-src/openblas
mkdir -p "$root/.deps/openblas"
make -C "$src" -j"${SLURM_CPUS_PER_TASK:-2}" CC=gcc FC=gfortran CFLAGS='-O3 -fPIC' FFLAGS='-O3 -fPIC' USE_THREAD=0 USE_OPENMP=0 USE_LOCKING=1 NO_AFFINITY=1 DYNAMIC_ARCH=1 > "$root/temp/mpi_bench/openblas-safe-build.log" 2>&1
make -C "$src" install PREFIX="$root/.deps/openblas" >> "$root/temp/mpi_bench/openblas-safe-build.log" 2>&1

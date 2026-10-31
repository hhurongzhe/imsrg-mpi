#!/bin/bash
set -euo pipefail
cd /scratch/hrz/imsrg-mpi
source tests/mpi/env.sh
out=$PWD/temp/mpi_bench/startup-check
mkdir -p "$out"
export OMP_NUM_THREADS=2 OPENBLAS_NUM_THREADS=1
# Reject the known-unsafe library before reading any force files.
if LD_LIBRARY_PATH="$IMSRG_OPT/openblas/lib:$LD_LIBRARY_PATH" build-mpi/imsrg++ method=HF > "$out/unsafe.log" 2>&1; then
  echo "Unsafe BLAS was not rejected" >&2; exit 1
fi
rg -q 'Unsafe BLAS configuration' "$out/unsafe.log"
# A safe library must still permit a small genuine HF calculation.
build-mpi/imsrg++ method=HF basis=HF 2bme=none 3bme=none reference=He4 valence_space=He4 emax=0 e3max=0 hw=16 "intfile=$out/hf" > "$out/safe.log" 2>&1
# Help must work even with the known-unsafe library.
LD_LIBRARY_PATH="$IMSRG_OPT/openblas/lib:$LD_LIBRARY_PATH" build-mpi/imsrg++ --help > "$out/help.log" 2>&1
if LD_LIBRARY_PATH="$IMSRG_OPT/openblas/lib:$LD_LIBRARY_PATH" mpirun --oversubscribe -np 2 --map-by package:PE=1 --bind-to core build-mpi/imsrg++ method=HF > "$out/unsafe-mpi.log" 2>&1; then
  echo "Unsafe BLAS was not rejected collectively" >&2; exit 1
fi
rg -q 'Unsafe BLAS configuration' "$out/unsafe-mpi.log"
OMP_NUM_THREADS=1 mpirun --oversubscribe -np 2 --map-by package:PE=1 --bind-to core build-mpi/imsrg++ method=HF basis=HF 2bme=none 3bme=none reference=He4 valence_space=He4 emax=0 e3max=0 hw=16 "intfile=$out/hf-mpi" > "$out/safe-mpi.log" 2>&1
if LD_LIBRARY_PATH="$IMSRG_OPT/openblas/lib:$LD_LIBRARY_PATH" temp/mpi_bench/build-root/src/imsrg++ method=HF > "$out/unsafe-serial.log" 2>&1; then
  echo "Serial executable did not reject unsafe BLAS" >&2; exit 1
fi
rg -q 'Unsafe BLAS configuration' "$out/unsafe-serial.log"
temp/mpi_bench/build-root/src/imsrg++ method=HF basis=HF 2bme=none 3bme=none reference=He4 valence_space=He4 emax=0 e3max=0 hw=16 "intfile=$out/hf-serial" > "$out/safe-serial.log" 2>&1
echo 'Startup safety checks PASS'

#!/bin/bash
# Alternating old/new communication on one allocation with shared warm caches.
set -euo pipefail
cd /scratch/hrz/imsrg-mpi
source tests/mpi/env.sh
root=$PWD/temp/mpi_bench/opt-20261031/batched
[[ -f "$root/REGRESSION_PASS" ]]
out=$root/paired-${SLURM_JOB_ID}_e${1:-10}
mkdir -p "$out/profile"
cmake -S tests/mpi/scaling/paired -B "$out/build" -DCMAKE_BUILD_TYPE=Release \
 -DIMSRG_LIBRARY_DIR="$root/build" \
 -DCMAKE_PREFIX_PATH="$IMSRG_OPT/openmpi;$IMSRG_OPT/gsl;$IMSRG_OPT/boost;$IMSRG_OPENBLAS" \
 -DBLAS_openblas_LIBRARY="$IMSRG_OPENBLAS/lib/libopenblas.so" > "$out/build.log" 2>&1
cmake --build "$out/build" -j4 >> "$out/build.log" 2>&1
export OMP_NUM_THREADS=4 OPENBLAS_NUM_THREADS=1 OMP_PROC_BIND=close OMP_PLACES=cores
export IMSRG_PROFILE_DIR=$out/profile
lscpu > "$out/lscpu.txt"
scontrol show hostnames "$SLURM_JOB_NODELIST" > "$out/nodes.txt"
mpirun --report-bindings -np "$SLURM_NTASKS" --map-by "ppr:${SLURM_NTASKS_PER_NODE}:node:PE=4" --bind-to core \
 "$out/build/paired-kernels" "${1:-10}" "${2:-012}" > "$out/console.log" 2>&1
touch "$out/COMPLETED"

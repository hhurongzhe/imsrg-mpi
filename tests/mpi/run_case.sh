#!/bin/bash
# Usage: run_case.sh executable ranks threads emax O16|O18 output-directory
set -euo pipefail
TIMEFORMAT=$'real %R\nuser %U\nsys %S'
exe=$(realpath "$1")
ranks=$2
export OMP_NUM_THREADS=$3 OPENBLAS_NUM_THREADS=1 OMP_PROC_BIND=close OMP_PLACES=cores
emax=$4
nucleus=$5
if [[ ! $emax =~ ^[0-6]$ ]]; then
  echo "The supplied NN benchmark fixture only supports emax <= 6." >&2
  exit 2
fi
if [[ $nucleus != O16 && $nucleus != O18 ]]; then
  echo "This benchmark harness supports O16 and O18." >&2
  exit 2
fi
dir=$(realpath -m "$6")
mkdir -p "$dir/scratch"
args=(
 "2bme=/scratch/hrz/imsrg-mpi/temp/mpi_bench/inputs/EM18_hw16_e6.me2j"
 "3bme=/scratch/hrz/imsrg-mpi/temp/mpi_bench/inputs/EM18_hw16_e6_E3_6.bin"
 file2e1max=6 file2e2max=12 file3e1max=6 file3e2max=12 file3e3max=6
 "emax=$emax" e3max=6 hw=16 "reference=$nucleus" method=magnus basis=HF "omega_norm_max=${BENCH_OMEGA_MAX:-0.25}"
 "flowfile=$dir/flow.dat" "intfile=$dir/result" "scratch=$dir/scratch"
)
if [[ $nucleus == O18 ]]; then args+=(valence_space=sd-shell Operators=Rp2,E2,M1); else args+=(valence_space=O16 Operators=Rp2); fi
if [[ $ranks == 0 ]]; then
  { time "$exe" "${args[@]}"; } > "$dir/console.log" 2>&1
else
  { time mpirun --oversubscribe --report-bindings -np "$ranks" --map-by "${IMSRG_MPI_MAP_BY:-slot:PE=$OMP_NUM_THREADS}" --bind-to core "$exe" "${args[@]}"; } > "$dir/console.log" 2>&1
fi

#!/bin/bash
#SBATCH --account=def-holt
#SBATCH --nodes=1
#SBATCH --ntasks=4
#SBATCH --cpus-per-task=2
#SBATCH --mem=24G
#SBATCH --time=01:00:00
#SBATCH --job-name=imsrg-mpi-validate
#SBATCH --output=/scratch/hrz/imsrg-mpi/temp/mpi_bench/validate-%j.log
set -euo pipefail
cd /scratch/hrz/imsrg-mpi
source tests/mpi/env.sh
out=$PWD/temp/mpi_bench/run-$SLURM_JOB_ID
mkdir -p "$out"
cp tests/mpi/run_case.sh "$out/run_case.sh"
cp tests/mpi/compare.py "$out/compare.py"
baseline=${BASELINE_BUILD:-$PWD/temp/mpi_bench/build-original}
mpi_build=${MPI_BUILD:-$PWD/build-mpi}
emax=${BENCH_EMAX:-4}
cores=${BENCH_CORES:-4}
export OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1
"$baseline/imsrg-original-check" 2 "$out/original" > "$out/kernel-original.log" 2>&1
for ranks in 1 2 4; do
  mpirun --oversubscribe -np "$ranks" --map-by package:PE=1 --bind-to core \
    "$mpi_build/imsrg-mpi-check" 2 "$out/mpi$ranks" > "$out/kernel-mpi$ranks.log" 2>&1
done
for nucleus in O16 O18; do
  bash "$out/run_case.sh" "$baseline/imsrg++" 0 "$cores" "$emax" "$nucleus" "$out/${nucleus}_e${emax}_original"
  for ranks in 1 2 4; do
    if (( cores % ranks )); then echo "BENCH_CORES must be divisible by each rank count" >&2; exit 1; fi
    bash "$out/run_case.sh" "$mpi_build/imsrg++" "$ranks" "$((cores/ranks))" "$emax" "$nucleus" "$out/${nucleus}_e${emax}_mpi$ranks"
  done
done
python3 "$out/compare.py" "$out" > "$out/comparison.txt"

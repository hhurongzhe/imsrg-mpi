#!/bin/bash
#SBATCH --account=def-holt
#SBATCH --nodes=1
#SBATCH --ntasks=4
#SBATCH --cpus-per-task=2
#SBATCH --mem=16G
#SBATCH --time=01:00:00
#SBATCH --job-name=imsrg-mpi-validate
#SBATCH --output=/scratch/hrz/imsrg-mpi/temp/mpi_bench/validate-%j.log
set -euo pipefail
cd /scratch/hrz/imsrg-mpi
exec bash tests/mpi/benchmark.slurm

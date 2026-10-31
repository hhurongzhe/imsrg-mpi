#!/bin/bash
#SBATCH --account=def-holt
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --mem=24G
#SBATCH --time=00:30:00
#SBATCH --job-name=imsrg-original-build
#SBATCH --output=/scratch/hrz/imsrg-mpi/temp/mpi_bench/original-build-%j.log
set -euo pipefail
cd /scratch/hrz/imsrg-mpi
source tests/mpi/env.sh
bash tests/mpi/build_baseline.sh

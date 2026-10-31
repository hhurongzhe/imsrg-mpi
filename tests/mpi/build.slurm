#!/bin/bash
#SBATCH --account=def-holt
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --mem=24G
#SBATCH --time=00:30:00
#SBATCH --job-name=imsrg-mpi-build
#SBATCH --output=/scratch/hrz/imsrg-mpi/temp/mpi_bench/build-%j.log
set -euo pipefail
cd /scratch/hrz/imsrg-mpi
source tests/mpi/env.sh
cmake --build build-mpi -j8

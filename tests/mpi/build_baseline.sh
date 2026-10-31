#!/bin/bash
set -euo pipefail
cd /scratch/hrz/imsrg-mpi
source tests/mpi/env.sh
python3 tests/mpi/prepare_baseline.py
cmake -S temp/mpi_bench/original-src -B temp/mpi_bench/build-original \
  -DIMSRG_BUILD_PYTHON_BINDINGS=OFF \
  -DCMAKE_PREFIX_PATH="$IMSRG_OPT/gsl;$IMSRG_OPT/boost;$IMSRG_OPENBLAS" \
  -DBLAS_openblas_LIBRARY="$IMSRG_OPENBLAS/lib/libopenblas.so"
cmake --build temp/mpi_bench/build-original -j"${SLURM_CPUS_PER_TASK:-2}"

#!/bin/bash
set -euo pipefail
cd /scratch/hrz/imsrg-mpi
source tests/mpi/env.sh
campaign=$PWD/temp/mpi_bench/scaling-20261030
mkdir -p "$campaign"
# Sources and helpers were frozen by the submission script before this job.
cmake -S "$campaign/source/src" -B "$campaign/build-mpi" \
  -DIMSRG_ENABLE_MPI=ON -DIMSRG_BUILD_MPI_TESTS=ON -DIMSRG_BUILD_PYTHON_BINDINGS=OFF \
  -DCMAKE_PREFIX_PATH="$IMSRG_OPT/openmpi;$IMSRG_OPT/gsl;$IMSRG_OPT/boost;$IMSRG_OPENBLAS" \
  -DBLAS_openblas_LIBRARY="$IMSRG_OPENBLAS/lib/libopenblas.so"
cmake --build "$campaign/build-mpi" -j8
cmake -S "$campaign/original-src" -B "$campaign/build-original" \
  -DIMSRG_BUILD_PYTHON_BINDINGS=OFF \
  -DCMAKE_PREFIX_PATH="$IMSRG_OPT/gsl;$IMSRG_OPT/boost;$IMSRG_OPENBLAS" \
  -DBLAS_openblas_LIBRARY="$IMSRG_OPENBLAS/lib/libopenblas.so"
cmake --build "$campaign/build-original" --target imsrg++ -j8
export OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1
ctest --test-dir "$campaign/build-mpi" --output-on-failure
# Test the instrumentation on a complete e4 original/MPI physical calculation.
mkdir -p "$campaign/smoke/O18_e4_original/profile" "$campaign/smoke/O18_e4_mpi4/profile"
IMSRG_PROFILE_DIR="$campaign/smoke/O18_e4_original/profile" bash "$campaign/source/tests/mpi/run_case.sh" "$campaign/build-original/imsrg++" 0 4 4 O18 "$campaign/smoke/O18_e4_original"
IMSRG_PROFILE_DIR="$campaign/smoke/O18_e4_mpi4/profile" bash "$campaign/source/tests/mpi/run_case.sh" "$campaign/build-mpi/imsrg++" 4 1 4 O18 "$campaign/smoke/O18_e4_mpi4"
python3 "$campaign/source/tests/mpi/compare.py" "$campaign/smoke" 4 > "$campaign/smoke/comparison.txt"
python3 "$campaign/source/tests/mpi/scaling/prepare_large_inputs.py" "$campaign/inputs"
ldd "$campaign/build-mpi/imsrg++" > "$campaign/libraries.txt"
touch "$campaign/READY"

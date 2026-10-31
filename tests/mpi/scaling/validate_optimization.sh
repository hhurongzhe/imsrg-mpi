#!/bin/bash
set -euo pipefail
cd /scratch/hrz/imsrg-mpi
source tests/mpi/env.sh
out=$PWD/temp/mpi_bench/opt-20261031
build=$out/build
cmake -S src -B "$build" -DIMSRG_ENABLE_MPI=ON -DIMSRG_BUILD_MPI_TESTS=ON -DIMSRG_BUILD_PYTHON_BINDINGS=OFF \
 -DCMAKE_PREFIX_PATH="$IMSRG_OPT/openmpi;$IMSRG_OPT/gsl;$IMSRG_OPT/boost;$IMSRG_OPENBLAS" -DBLAS_openblas_LIBRARY="$IMSRG_OPENBLAS/lib/libopenblas.so"
cmake --build "$build" -j8
export OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1
ctest --test-dir "$build" --output-on-failure
mkdir -p "$out/checks"
for p in temp/mpi_bench/safe-25431747/original_*.bin; do ln -sf "$PWD/$p" "$out/checks/$(basename "$p")"; done
for ranks in 2 4; do
 mpirun -np "$ranks" --map-by slot:PE=1 --bind-to core "$build/imsrg-mpi-check" 6 "$out/checks/mpi$ranks" > "$out/checks/mpi$ranks.log" 2>&1
done
python3 tests/mpi/compare.py "$out/checks" 2 4 > "$out/checks/comparison.txt"
mkdir -p "$out/physical"
# Independent original regression with final H, Rp2, E2, M1 outputs.
bash tests/mpi/run_case.sh temp/mpi_bench/scaling-20261030/build-original/imsrg++ 0 4 4 O18 "$out/physical/O18_e4_original"
bash tests/mpi/run_case.sh "$build/imsrg++" 4 2 4 O18 "$out/physical/O18_e4_mpi4"
python3 tests/mpi/compare.py "$out/physical" 4 > "$out/physical/comparison.txt"
touch "$out/REGRESSION_PASS"

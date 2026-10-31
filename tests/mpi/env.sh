# Source from build/test scripts; prefer the user's dependency stack.
IMSRG_OPT=/home/hrz/opt_setup/opt
IMSRG_PROJECT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
# Prefer the project-local rebuild with USE_THREAD=0 USE_LOCKING=1.
# Override explicitly with IMSRG_OPENBLAS_OVERRIDE when testing another BLAS.
if [[ -n ${IMSRG_OPENBLAS_OVERRIDE:-} ]]; then
  IMSRG_OPENBLAS="$IMSRG_OPENBLAS_OVERRIDE"
elif [[ -f "$IMSRG_PROJECT_ROOT/.deps/openblas/lib/libopenblas.so" ]]; then
  IMSRG_OPENBLAS="$IMSRG_PROJECT_ROOT/.deps/openblas"
else
  IMSRG_OPENBLAS="$IMSRG_OPT/openblas"
fi
export PATH="$IMSRG_OPT/openmpi/bin:$PATH"
export LD_LIBRARY_PATH="$IMSRG_OPT/openmpi/lib:$IMSRG_OPENBLAS/lib:$IMSRG_OPT/gsl/lib:$IMSRG_OPT/boost/lib:${LD_LIBRARY_PATH:-}"
export OPENBLAS_NUM_THREADS=1
# The login environment puts OpenMPI 4 headers in CPATH, ahead of CMake's
# -isystem OpenMPI 5 headers. Keep runtime dependencies, remove header mixing.
unset CPATH CPLUS_INCLUDE_PATH
export LIBRARY_PATH="$IMSRG_OPT/openmpi/lib:$IMSRG_OPENBLAS/lib:$IMSRG_OPT/gsl/lib:$IMSRG_OPT/boost/lib:${LIBRARY_PATH:-}"

# imsrg++
In-Medium Similarity Renormalization Group software for nuclear structure calculations. Written in C++, with Python bindings.
It is capable of performing Hartree-Fock, single-reference IM-SRG, and valence-space IM-SRG calculations.
If you don't know what these things are, you should check out
https://www.annualreviews.org/doi/10.1146/annurev-nucl-101917-021120
http://link.aps.org/doi/10.1103/PhysRevLett.106.222502
http://www.sciencedirect.com/science/article/pii/S0370157315005414

The IMSRG code in the src directory is copyrighted under the GNU Public License (http://www.gnu.org/licenses/gpl.html).
This code also uses the Armadillo library, which is covered under the Mozilla Public License (https://www.mozilla.org/en-US/MPL/2.0/), and the BOOST library, which is covered under the BOOST Software License (http://www.boost.org/users/license.html)


## Credits:
* Most of this code was written by Ragnar Stroberg, who benefitted greatly from the codes of Koshiroh Tsukiyama, Heiko Hergert, and Nathan Parzuchowski.
* A substantial amount of additional machinery was added by Takayuki Miyagi, including the HFMBPT, ThreeBodyMENO2B, and Pwd classes.
* Implementation of operators for neutrinoless double beta decay was done by Charlie Payne, and these were ported back to the main fork by Antoine Belley.
*  Some tweaks to the 3rd order MBPT code were contributed by Johannes Simonis.
* Implementation of the IMSRG(3f2) routines, and tensor IMSRG(3) commutators by Bingcheng He.

## MPI + OpenMP (IMSRG(2))

MPI is optional. On this cluster, first build the thread-safe copy of the
user's OpenBLAS source inside this project (on an allocated compute node):

```sh
bash tests/mpi/build_openblas_safe.sh
source tests/mpi/env.sh
cmake -S . -B build-mpi -DIMSRG_ENABLE_MPI=ON \
  -DIMSRG_BUILD_PYTHON_BINDINGS=OFF -DIMSRG_BUILD_MPI_TESTS=ON \
  -DCMAKE_PREFIX_PATH="$IMSRG_OPT/openmpi;$IMSRG_OPT/gsl;$IMSRG_OPT/boost;$IMSRG_OPENBLAS" \
  -DBLAS_openblas_LIBRARY="$IMSRG_OPENBLAS/lib/libopenblas.so"
cmake --build build-mpi -j4
ctest --test-dir build-mpi --output-on-failure
OMP_NUM_THREADS=2 OPENBLAS_NUM_THREADS=1 mpirun -np 4 \
  --map-by slot:PE=2 --bind-to core build-mpi/imsrg++ option=value ...
```

Allocate at least `MPI ranks × OMP_NUM_THREADS` physical cores. Set
`OPENBLAS_NUM_THREADS=1` to avoid nested BLAS parallelism. The local
`.deps/openblas` build uses the same OpenBLAS 0.3.33 source and dynamic kernels,
with `USE_THREAD=0 USE_LOCKING=1`. It does not change the installation under
`/home/hrz/opt_setup/opt`.

The existing OpenBLAS installation was built with `USE_THREAD=0` without
`USE_LOCKING=1`. Its concurrent DGEMM test produced 2,755 wrong results in
8,000 calls (`temp/mpi_bench/blas-threads-original.log`), and the unmodified
IMSRG code showed inconsistent multi-threaded emax=6 flows. Setting
`OPENBLAS_NUM_THREADS=1` alone does not make concurrent callers safe. The new
`BLASThreadSafety` CTest checks this dependency before accepting a build.
The executable also rejects a serial OpenBLAS build without locking when
multiple OpenMP threads are requested. Help and single-thread-per-rank runs
remain available. The project-local rebuild passed the same 8,000-call test
with zero errors (`temp/mpi_bench/safe-25431747/blas-threads.log`).

`tests/mpi/env.sh` prefers `.deps/openblas` and the user's OpenMPI/GSL/Boost.
Source it before configuring, building, or running here. `IMSRG_OPENBLAS_OVERRIDE` can
select another thread-safe BLAS installation. MPI-off remains the normal
standalone OpenMP build. The Python interface does not initialize this MPI
execution context; use the C++ executable for distributed runs.

Each rank holds the model space and full operators. The two-body channel work
in the scalar and tensor 122, pp/hh, and particle-hole commutators is distributed.
Tensor blocks use a deterministic weighted work assignment. Completed blocks
and Pandya intermediates are grouped by owning rank and broadcast before dependent
contractions. This preserves the summation order within a block and handles
coupled tensor channels. One-body work, HF, input loading, and normal ordering
are currently replicated. This implementation accelerates computation; it does
not divide the storage of the initial three-body interaction across ranks.
Use enough memory **per rank**, especially at large emax.

All ranks must execute the same calculation with the same arguments. Public
output is written by rank 0. Magnus scratch filenames include both PID and
rank; use a separate scratch directory for each independent run. MPI calls use
`MPI_THREAD_FUNNELED` and occur outside OpenMP regions. The multi-rank executable
rejects IMSRG(3), triples and goose-tank corrections, which are outside the
validated IMSRG(2) scope.

`tests/mpi/commutator.cc` compares all zero-, one-, and two-body elements of
scalar, rank-1, and rank-2 commutators, including fractional occupations and
Hermitian/anti-Hermitian cases, including two anti-Hermitian scalars in BCH products. `tests/mpi/benchmark.slurm` and `compare.py`
compare an independently built original code with 1/2/4 MPI ranks using the
EM1.8_2.0 NN+3N interaction, O16 single-reference and O18 sd-shell VS-IMSRG,
and Rp2/E2/M1 operators. The small supplied 3N file has E3max=6; this is a
regression/performance setup, not a converged physics prediction. Test outcomes
and timings must be checked in the run directory; successful compilation alone
does not establish numerical agreement or a speedup.

For the supplied NuHamil files, first run `python3 tests/mpi/prepare_inputs.py`.
Their raw binary layout differs from the original IMSRG `.bin` format. This
creates a lossless float64-to-text NN file and a full 3N file with the required
header in `temp/mpi_bench/inputs`; both versions read the same adapted data.
Do not feed the original headerless NN/full-3N files directly to the existing
`.bin` reader.

### Validated calculations (2026-10-30)

The original physics sources are independently built from a copy of
`/home/hrz/scratch/imsrg/src`; only that copy's CMake build setup is adapted.
All forces, original sources, and unrelated Slurm jobs remain untouched.

The following completed tests use EM1.8_2.0 NN+3N, `hw=16`, `E3max=6`, HF,
Magnus IMSRG(2), and the default `omega_norm_max=0.25`. O18 uses the sd valence
space and evolves Rp2, E2, and M1. Timings include reading, HF, the flow,
operator evolution, and output. These are individual runs on allocated nodes,
not a statistical scaling study.

| Case | Original, 1 OpenMP thread | MPI, 1 thread/rank | Observed speedup |
| --- | ---: | ---: | ---: |
| O16 SR, emax=6, Rp2 | 141.132 s | 4 ranks: 95.966 s | 1.47× |
| O18 VS, emax=6, Rp2/E2/M1 | 3394.466 s | 4 ranks: 970.361 s | 3.50× |
| O18 VS, emax=6, Rp2/E2/M1 | 3394.466 s | 8 ranks / 2 nodes: 693.771 s | 4.89× |

For O18 emax=6, all 334 printed flow rows agree; H, M1 and Rp2 output agrees
exactly, and E2 differs by at most 4.45e-16. See
`temp/mpi_bench/e6-final-comparison/comparison.txt`. O16 emax=6 also agrees,
including all 118 flow rows and Rp2. Independent original/MPI commutator tests
at emax=6 compare up to 2,514,253 elements per case, with maximum error 5.69e-14;
see `temp/mpi_bench/final-verify-25429329/comparison.txt`. O18 emax=4 H/Rp2/E2/M1
and all 284 flow rows also pass; see
`temp/mpi_bench/balanced-25428027/e4-comparison/`.

The failing emax=6 multi-threaded comparisons using the unsafe BLAS are
retained as diagnostics, not accepted validation results. The 1-thread-per-rank
results above avoid concurrent BLAS calls within a process.

With the project-local thread-safe BLAS, a same-node O18 emax=6 comparison
using `omega_norm_max=1.0` took 729.052 s for the original with 4 OpenMP
threads and 517.717 s for 2 MPI ranks × 2 threads: **1.41× at the same total
core count**. All 335 flow rows agree exactly; the largest H/Rp2/E2/M1 output
difference is 1.60e-14. Independent original/MPI scalar and rank-1/rank-2
kernel comparisons, including the anti-Hermitian/anti-Hermitian scalar case,
pass with maximum error 7.11e-14. See
`temp/mpi_bench/safe-25431747/comparison-mpi2.txt`. MPI-off and MPI-on CTests
also pass with this BLAS. A subsequent 4-rank × 2-thread run with the final
executable took 320.321 s (8 cores, on another node), with all 335 flow rows
identical and maximum H/Rp2/E2/M1 difference 5.33e-15; see
`temp/mpi_bench/safe-25431747/comparison.txt`. This is 2.28× relative to the
4-thread original, using twice the core count, not an equal-core comparison.
The emax=4 test did not benefit at equal core count
(original 4 threads: 49.196 s; 4 ranks × 1 thread: 65.491 s); communication
overhead can outweigh the parallel work at small model spaces.

The final executable's startup checks pass for MPI-on and MPI-off builds:
unsafe concurrent BLAS use is rejected, safe HF calculations run, and help
remains available. Two-rank rejection and safe HF execution are also checked
(`temp/mpi_bench/startup-check/check.log`).

For reproducible
new benchmarks, run `prepare_inputs.py`, `build_baseline.sh`, then submit
`benchmark.slurm`; `BENCH_EMAX` and `BENCH_CORES` select the model space and
total core count. The supplied NN fixture only covers emax <= 6; larger spaces
need larger force files. Keep both executables on the same thread-safe BLAS.

Storage is still replicated per rank, and the initial full 3N interaction is
not distributed. emax=8 and larger production spaces have not yet been
validated. The small-space speedups do not establish memory scaling or
performance at arbitrary rank counts. Optional `IMSRG_MPI_VALIDATE=1` checks
commutator metadata and operand norms across ranks; it was enabled in the
completed two-node O18 calculation.

### Larger-space performance campaign

The emax=10,12 campaign uses the full EM1.8_2.0 interaction with E3max=16,
O16 SR and O18 sd+pf VS calculations. It compares the original with 16 threads
against 1/4/10/20 MPI ranks × 4 threads, with separate scalar/tensor kernel
measurements and per-rank phase/communication profiling. These jobs are
submitted measurements, not yet evidence of e14 speedups. See
[the campaign protocol](tests/mpi/scaling/README.md) and
`temp/mpi_bench/scaling-20261030/jobs.json` for the precise setup and job IDs.
These long allocations and their dependent analysis were subsequently cancelled.
Current probes use 3-hour allocations; incomplete runs provide only phase timings.
Local `temp/` artifacts mentioned here are intentionally excluded from Git.

### Communication optimization (2026-10-31)

The IMSRG(2) pp/hh kernels now exchange only shared-orbit matrix elements of
Mpp/Mhh needed by the one-body contraction. Output blocks are accumulated by
their owners and synchronized once per complete parity/isospin-conserving
scalar or tensor commutator. Summation order is unchanged; standalone kernels
retain their result synchronization. Storage remains replicated. The structural
slice cache uses exact channel/ket metadata and is bounded to eight layouts.

Setting `IMSRG_MPI_FULL_INTERMEDIATES=1` on every rank restores the previous
communication path for controlled comparisons. Default operation enables the
optimization for multiple ranks. MPI-off/on tests, original-source emax=6
full-matrix comparisons, and complete O18 emax=4 H/Rp2/E2/M1 regressions pass;
all 284 printed flow rows match and the largest exported-operator difference
is 7.11e-15. Large-space physical results are still pending.

Alternating old/new paths on the same two nodes, with 20 ranks × 4 threads and
three warm samples per path, gives these median kernel times:

| Kernel | Previous path | Optimized path | Improvement |
| --- | ---: | ---: | ---: |
| emax=10 scalar | 2.4195 s | 1.6886 s | 1.43× |
| emax=10 J=1 tensor | 19.1333 s | 16.8495 s | 1.14× |
| emax=10 J=2 tensor | 33.3220 s | 28.9519 s | 1.15× |
| emax=12 scalar | 23.5846 s | 15.4752 s | 1.52× |

The paired full-element checks pass (maximum difference 4.55e-13). These are
improvements over the previous MPI implementation, not speedups over the
original OpenMP code. e12 scalar logical broadcast payload decreases from
7.919 GB to 4.466 GB per commutator. The Mpp/Mhh slices contain about 3.77% of
the full intermediates. Rank imbalance remains visible: in one e10 J=1 sample,
the optimized comm122st computation ranges from 2.87 to 7.96 s across ranks.

Results: `temp/mpi_bench/opt-20261031/batched/paired-summary.tsv`. The next full
campaign uses frozen binaries and a source archive in
`temp/mpi_bench/scaling-optimized-20261031`. Its `jobs.json` includes the retained
original baselines and the replacement optimized MPI jobs; the previous 16 MPI
physical jobs and their obsolete analysis job were cancelled before execution.


The long optimized campaign described above was subsequently cancelled at the
user's request. Only several-hour tests are retained; full e10/e12 physical
speedups remain unverified. The repository contains source, documentation, and
test scripts, but excludes interactions, generated outputs, build trees, and
local dependencies. Cluster-specific test scripts require adapting paths and
providing the interaction files separately.

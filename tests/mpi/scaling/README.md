# emax=10,12 performance campaign

Campaign: `temp/mpi_bench/scaling-20261030`. All physics sources, build inputs,
commands, rank bindings, node names, profiles, and Slurm job IDs are retained.
Original sources outside this project and unrelated jobs are never modified.

Status 2026-10-31: all ten kernel runs completed and passed their elementwise
checks. The twenty pending physical runs were held during optimization.
After original/MPI regressions and same-allocation performance comparisons
passed, the sixteen old MPI physical runs and analysis job 25437492 were
cancelled. Original baseline arrays 25437446 and 25437453 were retained and
released; optimized MPI replacements and analysis 25570440 use
`temp/mpi_bench/scaling-optimized-20261031`. The old frozen source and completed
kernel results remain the performance baseline. Optimization validation and
paired timings are recorded in `temp/mpi_bench/opt-20261031`.

Update: the user subsequently requested only several-hour test allocations.
All twenty 3/7-day physical runs and dependent analysis 25570440 were cancelled.
Three 3-hour e10 O18 VS probes (original OMP16, MPI4 x OMP4, MPI20 x OMP4)
replace the immediate measurements. Future submissions should use several-hour
limits. Incomplete trajectories are phase diagnostics, not full-flow speedups.
Paths under `temp/` below identify local artifacts, intentionally excluded from
Git; they are not bundled datasets or downloadable regression fixtures.

## Physical cases and allocation

* EM1.8_2.0 NN+full 3N, hw=16, E3max=16; NN file emax=12 and 3N file emax=14.
* O16 SR-IMSRG(2), Rp2; O18 VS-IMSRG(2), `sdfp-shell` (sd+pf), Rp2/E2/M1.
* Both cases at emax=10 and 12, HF basis, Magnus, omega_norm_max=0.25,
  eta_criterion=1e-6, smax=200 (VS second step follows the original doubling).
* Original: 16 OpenMP threads. MPI: 1,4,10,20 ranks, 4 threads/rank.
  Twenty ranks use two nodes. Original vs 4 MPI ranks is an equal-core test;
  MPI 1→20 uses a fixed thread count per rank. These are different comparisons.
* Ten arrays, two physical cases each; array concurrency one limits duplicate
  simultaneous force reads per configuration. The cancelled allocations used e10 limits of 3 days and e12 limits of 7 days.
  Request 48/64 GiB per rank respectively, retaining replicated storage.
* Five kernel arrays, e10 and e12 each: 1/4/10 ranks on one node, 20 ranks on
  one and two nodes. Fixed scalar/rank-1/rank-2 commutators use fractional
  occupations, a warm-up, three timed repeats, and an untimed full-element
  comparison against the same kernels with MPI distribution disabled on each rank.
  Kernel tests are synthetic fixed-work measurements, not physical flow results.

Original physics kernels are unchanged. Only CMake plumbing and identical
observational stage markers are added to the staged original main program.
Both builds use the same project-local thread-safe BLAS. The preparation job
builds independent binaries, runs CTests, compares complete O18 e4 outputs,
and losslessly adapts the large input files before releasing dependent jobs.
Main source snapshots are immutable after submission; analysis helpers may be
corrected before their dependent analysis task starts. `source-sha256.json`
records the prepared source snapshot; `jobs.json` records submitted commands.

## Measurement and interpretation

`IMSRG_PROFILE_DIR` enables one TSV per rank. Stage checkpoints flush cumulative
function timers and MPI metrics; `summarize.py` differences adjacent checkpoints
and writes rank min/mean/max to `phases.tsv`. Checkpoints introduce no collectives.
Measured stages cover model-space setup, NN/3N reading, Hamiltonian setup, HF,
normal ordering of H, operator setup/MBPT, core/valence flow, preparation and
Magnus evolution of each observable, and final output. Preparation of an
observable includes output/renormal-ordering remaining from the preceding stage.

Function timers are **inclusive and nested**; do not sum them as disjoint work.
Broadcast seconds include communication and waiting for other ranks; these
cannot alone identify network latency. `block_sync_seconds` includes broadcast;
`block_pack_copy_seconds` excludes time inside broadcast but includes allocation
and traversal. Bytes are logical payload processed by each rank, not measured
network traffic. Kernel sample time is the maximum elapsed across ranks; setup,
reference checks and barriers are excluded from that sample time. Checkpoint
phase time can include those harness operations; use the explicit KERNEL samples
for scaling. Compare the same logical phase across rank counts, then inspect
scalar/tensor 122, pp/hh, Pandya timers, broadcast and packing costs.

The current code replicates input, HF, normal ordering, and much one-body work.
Scalar channel ownership is cyclic; tensor ownership uses matrix-size weights.
These are **candidate** bottlenecks, not a measured ranking at e10/e12 yet.
A large spread of broadcast waiting times can also reflect unequal computation.
The same-/two-node 20-rank kernel pair helps distinguish topology effects.

`analyze_completed.py CAMPAIGN` creates:

* `summary.json`: completed/incomplete status and per-stage flow endpoints;
* `phases.tsv`: rank min/mean/max, including partially completed phases;
* `phase-scaling.tsv`: phase ratios against one rank, broadcast/waiting spread,
  and packing costs (partial phase comparisons are diagnostic, not accepted
  end-to-end speedups);
* `comparisons/*/comparison.txt`: original/MPI full-flow and operator checks;
* `scaling.tsv`: wall ratios and allocated-core-seconds ratios; accepted speedup
  requires completed, converged baseline and MPI runs plus matching outputs;
* `kernel-scaling.tsv`: min/median/max of the three warm kernel samples;
* `accounting.txt`: Slurm state, elapsed time, allocated CPUs, TotalCPU, MaxRSS.

The analysis job depends on all arrays ending (including failures). It does not
convert timeouts, numerical mismatches, or nonconvergence into successful speedups.
An interrupted run is not restartable just because Magnus scratch files exist.
Full physical runs are individual samples; kernel repeats estimate short-run
variation, not node-to-node statistical uncertainty. Large multi-shell valence
spaces can have convergence problems; inspect both flow stages before interpreting
wall time. No month-to-days claim for e14 follows from submission alone.

For equal work and fixed threads/rank, a 30-day→3-day target needs at least 10×
end-to-end speedup. An idealized Amdahl model with 20 ranks requires an effective
serial fraction below about 5.3%, even before communication overhead. A 2-day
result requires 15× and a fraction below about 1.8%. The actual original thread
count, nucleus and valence space must be matched before extrapolating these data.

## Initial diagnostic observations (partial, 2026-10-30)

The first e10 **scalar synthetic kernel** samples (4 threads/rank, three warm
repeats, different allocated nodes) have median times 2.3959 s at one rank,
2.1761 s at four ranks, and 1.6739 s at ten ranks: 1.10× and 1.43× relative to
one rank. Full-element scalar checks pass. J=2 cases and the complete
physical calculations are still pending/running; these are not end-to-end
speedups and do not establish the e12/e14 scaling curve.

In the middle repeat at ten ranks, mean broadcast-call time including waiting
is 0.765 s/rank and mean packing/copying time is 0.186 s/rank. These two metrics
are disjoint. The pp/hh commutator takes about 0.629 s mean at ten ranks versus
0.442 s at one rank. In contrast, inverse Pandya time decreases from 1.156 s
to about 0.286 s mean. Thus adding ranks is helping some arithmetic while
synchronization and replicated/memory work erase much of the benefit elsewhere.
The current scalar implementation synchronizes full Mpp/Mhh intermediates and
then replicates their use in the one-body contraction; tensor pp/hh additionally
synchronizes the output blocks. Reducing these intermediate transfers is a
concrete candidate for the next optimization experiment, with strict output
regression checks required before acceptance.

For the J=1 tensor middle repeat at ten ranks, `comm122st` accounts for about
10.25 s of 18.51 s total scalar-tensor time and particle-hole work for about
6.79 s. Mean broadcast/wait time is 5.65 s and the maximum rank observes 8.78 s.
These inclusive timings overlap; do not add them. The rank-dependent waiting
and matrix-size-only tensor scheduling warrant a compute-load analysis before
attributing the entire delay to the network. First finish J=2 and the 20-rank
same-/cross-node controls. Frozen raw evidence is under `runs/kernels_e10_*`.

The J=1 warm medians are now 101.2030 s (one rank), 32.2976 s (four ranks),
and 18.2790 s (ten ranks), corresponding to 3.13× and 5.54×. All use four
threads/rank and three timed samples. The four-/ten-rank J=1 full-element
checks have passed (maximum error about 1.14e-13). This stronger tensor scaling
must be kept separate from the weak scalar scaling; a whole VS calculation
weights both, plus setup and observable transformations. J=2 ten-rank warm
samples are about 33 s but its one-rank baseline is not yet available.

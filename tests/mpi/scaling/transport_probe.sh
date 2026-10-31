#!/bin/bash
set -euo pipefail
cd /scratch/hrz/imsrg-mpi
source tests/mpi/env.sh
out=$PWD/temp/mpi_bench/opt-20261031/transport-$SLURM_JOB_ID
mkdir -p "$out"
mpicxx -O2 tests/mpi/scaling/transport_probe.cc -o "$out/probe"
srun --mpi=none --ntasks=2 --ntasks-per-node=1 --cpus-per-task=1 bash -c 'hostname; ls /sys/class/infiniband /dev/infiniband' > "$out/devices.log" 2>&1 || true
mpirun -np 2 --map-by ppr:1:node --bind-to core --mca pml_base_verbose 20 "$out/probe" > "$out/default.log" 2>&1
if mpirun -np 2 --map-by ppr:1:node --bind-to core --mca pml ucx "$out/probe" > "$out/ucx.log" 2>&1; then
 echo UCX_PASS > "$out/status.txt"
else
 echo UCX_UNAVAILABLE > "$out/status.txt"
fi

#!/bin/bash
# Frozen by the campaign before submission. mode emax ranks threads
set -euo pipefail
cd /scratch/hrz/imsrg-mpi
source tests/mpi/env.sh
campaign=$PWD/temp/mpi_bench/scaling-20261030
mode=$1; emax=$2; ranks=$3; threads=$4
[[ -f "$campaign/READY" ]]
export OMP_NUM_THREADS=$threads OPENBLAS_NUM_THREADS=1 OMP_PROC_BIND=close OMP_PLACES=cores
if [[ $mode == full ]]; then
  if [[ ${SLURM_ARRAY_TASK_ID:-0} == 0 ]]; then case_name=O16_SR; ref=O16; val=O16; ops=Rp2; else case_name=O18_VS_sdfp; ref=O18; val=sdfp-shell; ops=Rp2,E2,M1; fi
else
  case_name=kernels
  emax=$((10+2*${SLURM_ARRAY_TASK_ID:-0}))
fi
out=$campaign/runs/${case_name}_e${emax}_r${ranks}_t${threads}_n${SLURM_JOB_NUM_NODES}
mkdir -p "$out/profile" "$out/scratch"
export IMSRG_PROFILE_DIR=$out/profile
printf '%s\n' "$SLURM_JOB_ID" > "$out/jobid"
lscpu > "$out/lscpu.txt"
scontrol show hostnames "$SLURM_JOB_NODELIST" > "$out/nodes.txt"
exe=$campaign/build-mpi/imsrg++
if [[ $ranks == 0 ]]; then exe=$campaign/build-original/imsrg++; fi
if [[ $mode == kernels ]]; then
  exe=$campaign/build-mpi/imsrg-scaling-kernels
  args=("$emax")
else
  args=("2bme=$campaign/inputs/EM18_hw16_e12.me2j" "3bme=$campaign/inputs/EM18_hw16_e14_E3_16.bin"
    file2e1max=12 file2e2max=24 file3e1max=14 file3e2max=28 file3e3max=16
    "emax=$emax" e3max=16 hw=16 "reference=$ref" "valence_space=$val" "Operators=$ops"
    method=magnus basis=HF omega_norm_max=0.25 eta_criterion=1e-6 smax=200
    "flowfile=$out/flow.dat" "intfile=$out/result" "scratch=$out/scratch")
fi
printf '%q ' "$exe" "${args[@]}" > "$out/command.txt"
printf '\n' >> "$out/command.txt"
start=$(date +%s)
trap 'code=$?; printf "exit_code=%s\nwall_seconds=%s\n" "$code" "$(( $(date +%s) - start ))" > "$out/status.txt"' EXIT
TIMEFORMAT=$'real %R\nuser %U\nsys %S'
if [[ $ranks == 0 ]]; then
  { time "$exe" "${args[@]}"; } > "$out/console.log" 2>&1
else
  { time mpirun --report-bindings -np "$ranks" --map-by "ppr:${SLURM_NTASKS_PER_NODE}:node:PE=$threads" --bind-to core "$exe" "${args[@]}"; } > "$out/console.log" 2>&1
fi
touch "$out/COMPLETED"

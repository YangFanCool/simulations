#!/bin/bash

if [ $# -eq 1 ] && [ "$1" == "clean" ]; then
    rm -r Back* *log* 2>/dev/null
    rm -r "1/" "2/" "3/" 2>/dev/null
    echo "Cleanup completed."
    exit 0
fi

if [ $# -gt 2 ]; then
    echo "Usage: $0 <config_number> [cuda_device] or $0 clean"
    exit 1
fi

config=$1
cuda_device=${2:-0}

export CUDA_VISIBLE_DEVICES=$cuda_device

rm -r Back* *log* 2>/dev/null
rm -r "$config/" 2>/dev/null
mkdir "$config"
nohup ./Nyx3d.gnu.TPROF.MPI.CUDA.ex "./configs/$config" > "$config/run.log" 2>&1 &

echo "Running with config $config on CUDA device $cuda_device ..."
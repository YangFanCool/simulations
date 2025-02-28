#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: $0 <config_number>"
    exit 1
fi

config=$1

rm -r Back* *log* 2>/dev/null
rm -r "$config/" 2>/dev/null
mkdir "$config"
nohup ./Nyx3d.gnu.TPROF.MPI.CUDA.ex "./configs/$config" > "$config/run.log" 2>&1 &

echo "Running with config $config ..."
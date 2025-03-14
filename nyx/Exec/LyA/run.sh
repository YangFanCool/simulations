#!/bin/bash
rm -r Back* *log* 2>/dev/null

if [ $# -eq 1 ] && [ "$1" -eq 0 ]; then
    pkill -f 'Nyx'
    rm -r "1/" "2/" 2>/dev/null
    echo "Cleanup completed."
    exit 0
fi

if [ $# -eq 1 ] && [ "$1" -eq 1 ]; then
    rm -rf /root/autodl-tmp/fan/nyx_128_per_100 2>/dev/null
    mkdir /root/autodl-tmp/fan/nyx_128_per_100
fi

if [ $# -eq 1 ] && [ "$1" -eq 2 ]; then
    rm -rf /root/autodl-tmp/fan/nyx_128_per_10 2>/dev/null
    mkdir /root/autodl-tmp/fan/nyx_128_per_10
fi

config=$1
cuda_device=$((config - 1))
export CUDA_VISIBLE_DEVICES=$cuda_device

rm -r "$config/" 2>/dev/null
mkdir "$config"
cp "./configs/$config" "./$config/params"
nohup ./Nyx3d.gnu.TPROF.MPI.CUDA.ex "./configs/$config" > "$config/run.log" 2>&1 &

echo "Running with config $config on CUDA device $cuda_device ..."
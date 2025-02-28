rm -r *log* *out* *Back* *plt* *chk* 2>/dev/null

nohup ./Nyx3d.gnu.TPROF.MPI.CUDA.ex inputs > run.log 2>&1 &
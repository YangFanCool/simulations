rm -r Back* 2>/dev/null

rm -r *log* *out*  2>/dev/null

rm -r  *plt* 2>/dev/null

rm -r  *chk* 2>/dev/null

nohup ./Nyx3d.gnu.TPROF.MPI.CUDA.ex ./configs/1 > run.log 2>&1 &
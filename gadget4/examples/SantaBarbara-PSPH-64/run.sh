 #!/bin/bash
rm -r output 2>/dev/null

mkdir output

nohup mpirun -np 32 ./Gadget4 ./param.txt  > "output/run.log" 2>&1 &
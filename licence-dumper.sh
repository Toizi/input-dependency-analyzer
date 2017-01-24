clang -emit-llvm -c licence.c -o licence.bc 
llvm-slicer -c logHash licence.bc -o licence.sliced
./compare-plot-cfgs.sh licence.bc licence.sliced main


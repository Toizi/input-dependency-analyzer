clang -emit-llvm -c licence.c -o licence.bc 
opt -load build/OH/libOHPass.so licence.bc -oh -o licence.oh
llvm-slicer -c logHash licence.oh -o licence.sliced
./compare-plot-cfgs.sh licence.oh licence.sliced main licenceCheck


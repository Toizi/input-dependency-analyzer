# $1 is the input file 
# $2 is the function to dump
clang -emit-llvm -c $1.c -o $1.bc 
clang -emit-llvm -c rtlib-short.c -o rtlib-short.bc
clang -emit-llvm -c rtlib-short2.c -o rtlib-short2.bc
opt -load build/OH/libOHPass.so $1.bc -oh -o $1.oh
llvm-link $1.oh rtlib.bc -o $1.oh
llvm-slicer -c logHash $1.oh -o $1.sliced
llvm-link $1.oh rtlib-short2.bc -o $1.oh
llvm-link $1.sliced rtlib-short.bc -o $1.sliced
./compare-plot-cfgs.sh $1.oh $1.sliced main $2
lli $1.oh
lli $1.sliced

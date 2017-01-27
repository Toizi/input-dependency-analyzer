clang -emit-llvm -c rtlib.c -o rtlib.bc
clang -emit-llvm -c rtlib-short.c -o rtlib-short.bc
clang -emit-llvm -c $1.c -o $1.bc          
opt -load build/OH/libOHPass.so $1.bc -oh -o $1.oh
llvm-link $1.oh rtlib.bc -o $1.oh
lli $1.oh

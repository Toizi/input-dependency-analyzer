llvm-slicer -c logHash $1.oh -o $1.verifier
#llvm-link $1.oh rtlib-short2.bc -o $1.oh
llvm-link $1.verifier rtlib-short.bc -o $1.verifier


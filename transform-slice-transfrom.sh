echo "build all"
cd build
make
#sleep 5
cd ..
#first emit llvm from c code
echo "emit llvm bc from c code"
clang -emit-llvm -c $1.c -o $1.bc
#do OH transformation
echo "1st OH transformation"
opt -load build/OH/libOHPass.so $1.bc -oh -o $1.oh
#opt -load build/DumpHash/libDumpHashPass.so $1.oh -dump-hash -o $1.oh
echo "slice for the hash parameter"
llvm-slicer -c logHash $1.oh -o $1.oh.sliced
./compare-plot-cfgs.sh $1.oh $1.oh.sliced main
#slicer does not count for any read operations so we have to inject hashing instructions again
#echo "2nd OH transfrmation"
#opt -load build/OH/libOHPass.so $1.oh.sliced -oh -o $1.verifier
#dump the hash for testing purpose of the verifier
#echo "Inject hash dumper"
#opt -load build/DumpHash/libDumpHashPass.so $1.verifier -dump-hash -o $1.verifier.dh
#opt -load build/DumpHash/libDumpHashPass.so $1.oh.sliced -dump-hash -o $1.verifier
#link rtlib with the verifier
#echo "linking with rtlib"
#llvm-link $1.verifier.dh rtlib.bc -o $1.verifier.dh
#llvm-link $1.verifier rtlib.bc -o $1.verifier
#link oh for testing
#llvm-link $1.oh rtlib.bc -o $1.oh
#run the verifier
#echo "run $1.verifier.dh"
#lli $1.verifier.dh
#lli $1.verifier>qs.verifier.re
#lli $1.oh>qs.oh.re



./reproduce/validate.sh || exit -1

# Building clang++ is required

# cd build
# cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DLLVM_ENABLE_PROJECTS="libcxx;libcxxabi;clang" -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" ../llvm
# make -j48

CLANG_DIR=~/workspace/llvm-project/build ${CLANG_DIR}/bin/clang++ -fcoroutines-ts -g0 -O3 -march=native -std=c++2a -mllvm -inline-threshold=50000 coroutine/reuse.cpp -stdlib=libc++ -nostdinc++ -I${CLANG_DIR}/include/c++/v1 -L${CLANG_DIR}/lib -Wl,-rpath,${CLANG_DIR}/lib -DNDEBUG=1

for GROUP_SIZE in `seq 1 100`; do
  ./a.out 1e7 1e7 ${GROUP_SIZE} normal nvm /mnt/pmem0/renen | tee -a results/coroutines.txt
  ./a.out 1e7 1e7 ${GROUP_SIZE} coro nvm /mnt/pmem0/renen | tee -a results/coroutines.txt
  ./a.out 1e7 1e7 ${GROUP_SIZE} normal ram /mnt/pmem0/renen | tee -a results/coroutines.txt
  ./a.out 1e7 1e7 ${GROUP_SIZE} coro ram /mnt/pmem0/renen | tee -a results/coroutines.txt
done
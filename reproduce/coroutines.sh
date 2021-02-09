./reproduce/validate.sh || exit -1

# Building clang++ is required

# cd build
# cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DLLVM_ENABLE_PROJECTS="libcxx;libcxxabi;clang" -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" ../llvm
# make -j48

echo "" > results/coroutines.txt

export CLANG_DIR=~/workspace/llvm-project/build

${CLANG_DIR}/bin/clang++ -fcoroutines-ts -g0 -O3 -march=native -std=c++2a -mllvm -inline-threshold=50000 coroutine/coro_insert.cpp -stdlib=libc++ -nostdinc++ -I${CLANG_DIR}/include/c++/v1 -L${CLANG_DIR}/lib -Wl,-rpath,${CLANG_DIR}/lib -DNDEBUG=1 || exit

for GROUP_SIZE in 1 2 3 4 5 6 7 8 10 12 14 16 24 32 40 48 56 64; do
  ./a.out 1e7 1e7 ${GROUP_SIZE} nvm /mnt/pmem0/renen | tee -a results/coroutines.txt
  ./a.out 1e7 1e7 ${GROUP_SIZE} ram /mnt/pmem0/renen | tee -a results/coroutines.txt
done

${CLANG_DIR}/bin/clang++ -fcoroutines-ts -g0 -O3 -march=native -std=c++2a -mllvm -inline-threshold=50000 coroutine/coro_lookup.cpp -stdlib=libc++ -nostdinc++ -I${CLANG_DIR}/include/c++/v1 -L${CLANG_DIR}/lib -Wl,-rpath,${CLANG_DIR}/lib -DNDEBUG=1 || exit

for GROUP_SIZE in 1 2 3 4 5 6 7 8 10 12 14 16 24 32 40 48 56 64; do
  ./a.out 1e7 1e7 ${GROUP_SIZE} nvm /mnt/pmem0/renen | tee -a results/coroutines.txt
  ./a.out 1e7 1e7 ${GROUP_SIZE} ram /mnt/pmem0/renen | tee -a results/coroutines.txt
done

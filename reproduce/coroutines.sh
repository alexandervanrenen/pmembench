./reproduce/validate.sh || exit -1

# Building clang++ is required

# cd build
# cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DLLVM_ENABLE_PROJECTS="libcxx;libcxxabi;clang" -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" ../llvm
# make -j48

echo "" > results/coroutines.txt

export CLANG_DIR=~/workspace/llvm-project/build

${CLANG_DIR}/bin/clang++ -fcoroutines-ts -g0 -O3 -march=native -std=c++2a -mllvm -inline-threshold=50000 coroutine/coro_insert.cpp -stdlib=libc++ -nostdinc++ -I${CLANG_DIR}/include/c++/v1 -L${CLANG_DIR}/lib -Wl,-rpath,${CLANG_DIR}/lib -DNDEBUG=1 || exit

for GROUP_SIZE in 1 2 3 4 5 6 7 8 10 12 16 24 32 64; do
  ./a.out 1e7 1e7 ${GROUP_SIZE} nvm /mnt/pmem0/renen | tee -a results/coroutines.txt
  ./a.out 1e7 1e7 ${GROUP_SIZE} ram /mnt/pmem0/renen | tee -a results/coroutines.txt
done

${CLANG_DIR}/bin/clang++ -fcoroutines-ts -g0 -O3 -march=native -std=c++2a -mllvm -inline-threshold=50000 coroutine/coro_lookup.cpp -stdlib=libc++ -nostdinc++ -I${CLANG_DIR}/include/c++/v1 -L${CLANG_DIR}/lib -Wl,-rpath,${CLANG_DIR}/lib -DNDEBUG=1 || exit

for GROUP_SIZE in 1 2 3 4 5 6 7 8 10 12 16 24 32 64; do
  ./a.out 1e7 1e7 ${GROUP_SIZE} nvm /mnt/pmem0/renen | tee -a results/coroutines.txt
  ./a.out 1e7 1e7 ${GROUP_SIZE} ram /mnt/pmem0/renen | tee -a results/coroutines.txt
done

exit

cat damoncode/results/coroutines.txt | awk '/res: / && /insert_count/ && /coro_style: scalar/ && /use_ram: yes/ {print $NF}' > /tmp/insert_ram_scalar
cat damoncode/results/coroutines.txt | awk '/res: / && /insert_count/ && /coro_style: coro_write/ && /use_ram: yes/ {print $NF}' > /tmp/insert_ram_write
cat damoncode/results/coroutines.txt | awk '/res: / && /insert_count/ && /coro_style: coro_read/ && /use_ram: yes/ {print $NF}' > /tmp/insert_ram_read
cat damoncode/results/coroutines.txt | awk '/res: / && /insert_count/ && /coro_style: coro_full/ && /use_ram: yes/ {print $NF}' > /tmp/insert_ram_full

cat damoncode/results/coroutines.txt | awk '/res: / && /insert_count/ && /coro_style: scalar/ && /use_ram: no/ {print $NF}' > /tmp/insert_nvm_scalar
cat damoncode/results/coroutines.txt | awk '/res: / && /insert_count/ && /coro_style: coro_write/ && /use_ram: no/ {print $NF}' > /tmp/insert_nvm_write
cat damoncode/results/coroutines.txt | awk '/res: / && /insert_count/ && /coro_style: coro_read/ && /use_ram: no/ {print $NF}' > /tmp/insert_nvm_read
cat damoncode/results/coroutines.txt | awk '/res: / && /insert_count/ && /coro_style: coro_full/ && /use_ram: no/ {print $NF}' > /tmp/insert_nvm_full

paste /tmp/insert_ram_scalar /tmp/insert_ram_write /tmp/insert_ram_read /tmp/insert_ram_full /tmp/insert_nvm_scalar /tmp/insert_nvm_write /tmp/insert_nvm_read /tmp/insert_nvm_full

echo "------------------"

cat damoncode/results/coroutines.txt | awk '/res: / && /lookup_count/ && /coro_style: scalar/ && /use_ram: yes/ {print $NF}' > /tmp/lookup_ram_scalar
cat damoncode/results/coroutines.txt | awk '/res: / && /lookup_count/ && /coro_style: coro_read/ && /use_ram: yes/ {print $NF}' > /tmp/lookup_ram_read

cat damoncode/results/coroutines.txt | awk '/res: / && /lookup_count/ && /coro_style: scalar/ && /use_ram: no/ {print $NF}' > /tmp/lookup_nvm_scalar
cat damoncode/results/coroutines.txt | awk '/res: / && /lookup_count/ && /coro_style: coro_read/ && /use_ram: no/ {print $NF}' > /tmp/lookup_nvm_read

paste /tmp/lookup_ram_scalar /tmp/lookup_ram_read /tmp/lookup_nvm_scalar /tmp/lookup_nvm_read
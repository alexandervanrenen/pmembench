./reproduce/validate.sh || exit -1

# Reproduces data for figure 1
# PMem Bandwidth: Varying Access Granularity

echo "" > results/bw_cache_lines.txt

CXX=clang++

thread_count=24
for block_size in `seq 64 64 768`; do
   # Read nvm
   $CXX -g0 -O3 -march=native -std=c++14 bandwidth/bw.cpp -pthread -DBLOCK_SIZE=$block_size && ./a.out 1e9 $thread_count nvm ${PMEM_PATH} | tee -a results/bw_cache_lines.txt

   # Read ram
   $CXX -g0 -O3 -march=native -std=c++14 bandwidth/bw.cpp -pthread -DBLOCK_SIZE=$block_size && ./a.out 1e9 $thread_count ram ${PMEM_PATH} | tee -a results/bw_cache_lines.txt

   # Write nvm
   $CXX -g0 -O3 -march=native -std=c++14 bandwidth/bw.cpp -pthread -DBLOCK_SIZE=$block_size -DWRITE=1 && ./a.out 1e9 $thread_count nvm ${PMEM_PATH} | tee -a results/bw_cache_lines.txt
   $CXX -g0 -O3 -march=native -std=c++14 bandwidth/bw.cpp -pthread -DBLOCK_SIZE=$block_size -DWRITE=1 -DUSE_CLWB=1 && ./a.out 1e9 $thread_count nvm ${PMEM_PATH} | tee -a results/bw_cache_lines.txt
   $CXX -g0 -O3 -march=native -std=c++14 bandwidth/bw.cpp -pthread -DBLOCK_SIZE=$block_size -DWRITE=1 -DSTREAMING=1 && ./a.out 1e9 $thread_count nvm ${PMEM_PATH} | tee -a results/bw_cache_lines.txt

   # Write ram
   $CXX -g0 -O3 -march=native -std=c++14 bandwidth/bw.cpp -pthread -DBLOCK_SIZE=$block_size -DWRITE=1 && ./a.out 1e9 $thread_count ram ${PMEM_PATH} | tee -a results/bw_cache_lines.txt
   $CXX -g0 -O3 -march=native -std=c++14 bandwidth/bw.cpp -pthread -DBLOCK_SIZE=$block_size -DWRITE=1 -DUSE_CLWB=1 && ./a.out 1e9 $thread_count ram ${PMEM_PATH} | tee -a results/bw_cache_lines.txt
   $CXX -g0 -O3 -march=native -std=c++14 bandwidth/bw.cpp -pthread -DBLOCK_SIZE=$block_size -DWRITE=1 -DSTREAMING=1 && ./a.out 1e9 $thread_count ram ${PMEM_PATH} | tee -a results/bw_cache_lines.txt
done;

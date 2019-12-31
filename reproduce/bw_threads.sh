./reproduce/validate.sh || exit -1

# Reproduces data for figure 2
# PMem Bandwidth: Varying Thread Count

echo "" > results/bw_threads.txt

CXX=clang++

block_size=256
for thread_count in `seq 1 30`; do
   # Read nvm
   $CXX -g0 -O3 -march=native -std=c++14 bandwidth/bw.cpp -pthread -DBLOCK_SIZE=$block_size && ./a.out 1e9 $thread_count nvm ${PMEM_PATH} | tee -a results/bw_threads.txt

   # Read ram
   $CXX -g0 -O3 -march=native -std=c++14 bandwidth/bw.cpp -pthread -DBLOCK_SIZE=$block_size && ./a.out 1e9 $thread_count ram ${PMEM_PATH} | tee -a results/bw_threads.txt

   # Write nvm
   $CXX -g0 -O3 -march=native -std=c++14 bandwidth/bw.cpp -pthread -DBLOCK_SIZE=$block_size -DWRITE=1 && ./a.out 1e9 $thread_count nvm ${PMEM_PATH} | tee -a results/bw_threads.txt
   $CXX -g0 -O3 -march=native -std=c++14 bandwidth/bw.cpp -pthread -DBLOCK_SIZE=$block_size -DWRITE=1 -DUSE_CLWB=1 && ./a.out 1e9 $thread_count nvm ${PMEM_PATH} | tee -a results/bw_threads.txt
   $CXX -g0 -O3 -march=native -std=c++14 bandwidth/bw.cpp -pthread -DBLOCK_SIZE=$block_size -DWRITE=1 -DSTREAMING=1 && ./a.out 1e9 $thread_count nvm ${PMEM_PATH} | tee -a results/bw_threads.txt

   # Write ram
   $CXX -g0 -O3 -march=native -std=c++14 bandwidth/bw.cpp -pthread -DBLOCK_SIZE=$block_size -DWRITE=1 && ./a.out 1e9 $thread_count ram ${PMEM_PATH} | tee -a results/bw_threads.txt
   $CXX -g0 -O3 -march=native -std=c++14 bandwidth/bw.cpp -pthread -DBLOCK_SIZE=$block_size -DWRITE=1 -DUSE_CLWB=1 && ./a.out 1e9 $thread_count ram ${PMEM_PATH} | tee -a results/bw_threads.txt
   $CXX -g0 -O3 -march=native -std=c++14 bandwidth/bw.cpp -pthread -DBLOCK_SIZE=$block_size -DWRITE=1 -DSTREAMING=1 && ./a.out 1e9 $thread_count ram ${PMEM_PATH} | tee -a results/bw_threads.txt
done;

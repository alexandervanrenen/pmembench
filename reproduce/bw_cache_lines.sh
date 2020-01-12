./reproduce/validate.sh || exit -1

# Reproduces data for figure 1
# PMem Bandwidth: Varying Access Granularity

echo "" > results/bw_cache_lines.txt

COMPILE="clang++ -g0 -O3 -march=native -std=c++14 bandwidth/bw.cpp -pthread"

BYTE_COUNT=10e9
THREAD_COUNT=24
for BLOCK_SIZE in `seq 64 64 768`; do

  ${COMPILE} -DBLOCK_SIZE=${BLOCK_SIZE} -o a1.out || exit -1
  ${COMPILE} -DBLOCK_SIZE=${BLOCK_SIZE} -DWRITE=1 -o a2.out || exit -1
  ${COMPILE} -DBLOCK_SIZE=${BLOCK_SIZE} -DWRITE=1 -DUSE_CLWB=1 -o a3.out || exit -1
  ${COMPILE} -DBLOCK_SIZE=${BLOCK_SIZE} -DWRITE=1 -DSTREAMING=1 -o a4.out || exit -1

   # Read nvm
   ./a1.out ${BYTE_COUNT} ${THREAD_COUNT} nvm ${PMEM_PATH} | tee -a results/bw_cache_lines.txt

   # Read ram
   ./a1.out ${BYTE_COUNT} ${THREAD_COUNT} ram ${PMEM_PATH} | tee -a results/bw_cache_lines.txt

   # Write nvm
   ./a2.out ${BYTE_COUNT} ${THREAD_COUNT} nvm ${PMEM_PATH} | tee -a results/bw_cache_lines.txt
   ./a3.out ${BYTE_COUNT} ${THREAD_COUNT} nvm ${PMEM_PATH} | tee -a results/bw_cache_lines.txt
   ./a4.out ${BYTE_COUNT} ${THREAD_COUNT} nvm ${PMEM_PATH} | tee -a results/bw_cache_lines.txt

   # Write ram
   ./a2.out ${BYTE_COUNT} ${THREAD_COUNT} ram ${PMEM_PATH} | tee -a results/bw_cache_lines.txt
   ./a3.out ${BYTE_COUNT} ${THREAD_COUNT} ram ${PMEM_PATH} | tee -a results/bw_cache_lines.txt
   ./a4.out ${BYTE_COUNT} ${THREAD_COUNT} ram ${PMEM_PATH} | tee -a results/bw_cache_lines.txt
done;

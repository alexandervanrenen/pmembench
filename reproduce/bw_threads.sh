./reproduce/validate.sh || exit -1

# Reproduces data for figure 2
# PMem Bandwidth: Varying Thread Count

echo "" > results/bw_threads.txt

COMPILE="clang++ -g0 -O3 -march=native -std=c++14 -DNDEBUG=1 bandwidth/bw.cpp -pthread"

BYTE_COUNT=10e9

for BLOCK_SIZE in 256 1048576; do
  ${COMPILE} -DBLOCK_SIZE=${BLOCK_SIZE} -o a1.out || exit -1
  ${COMPILE} -DBLOCK_SIZE=${BLOCK_SIZE} -DWRITE=1 -o a2.out || exit -1
  ${COMPILE} -DBLOCK_SIZE=${BLOCK_SIZE} -DWRITE=1 -DUSE_CLWB=1 -o a3.out || exit -1
  ${COMPILE} -DBLOCK_SIZE=${BLOCK_SIZE} -DWRITE=1 -DSTREAMING=1 -o a4.out || exit -1

  for THREAD_COUNT in `seq 1 30`; do
     # Read nvm
     ./a1.out ${BYTE_COUNT} ${THREAD_COUNT} nvm ${PMEM_PATH} | tee -a results/bw_threads.txt

     # Read ram
     ./a1.out ${BYTE_COUNT} ${THREAD_COUNT} ram ${PMEM_PATH} | tee -a results/bw_threads.txt

     # Write nvm
     ./a2.out ${BYTE_COUNT} ${THREAD_COUNT} nvm ${PMEM_PATH} | tee -a results/bw_threads.txt
     ./a3.out ${BYTE_COUNT} ${THREAD_COUNT} nvm ${PMEM_PATH} | tee -a results/bw_threads.txt
     ./a4.out ${BYTE_COUNT} ${THREAD_COUNT} nvm ${PMEM_PATH} | tee -a results/bw_threads.txt

     # Write ram
     ./a2.out ${BYTE_COUNT} ${THREAD_COUNT} ram ${PMEM_PATH} | tee -a results/bw_threads.txt
     ./a3.out ${BYTE_COUNT} ${THREAD_COUNT} ram ${PMEM_PATH} | tee -a results/bw_threads.txt
     ./a4.out ${BYTE_COUNT} ${THREAD_COUNT} ram ${PMEM_PATH} | tee -a results/bw_threads.txt
  done;
done;
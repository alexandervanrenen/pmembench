./reproduce/validate.sh || exit -1

# Reproduces data for figure 5
# Page flush
PAGE_COUNT=100000 # ~1.6GB
REPETITIONS=10

# Experiment 1: 1 thread, _x_ dirty cls, streaming
echo "" > results/page_flush_cls_t1.txt
clang++ page_flush/page_flush.cpp -std=c++17 -g0 -O3  -march=native -DNDEBUG -DSTREAMING=1 -Invml/src/include/ nvml/src/nondebug/libpmem.a nvml/src/nondebug/libpmemblk.a -lpthread -lndctl -ldaxctl
THREAD_COUNT=1
for DIRTY_CL_COUNT in `seq 4 4 256`; do
  ./a.out ${PAGE_COUNT} ${DIRTY_CL_COUNT} ${THREAD_COUNT} ${REPETITIONS} ${PMEM_PATH}/file | tee -a results/page_flush_cls_t1.txt
done

# Experiment 2: _x_ thread, 16 dirty cls, streaming
echo "" > results/page_flush_threads_16cls.txt
clang++ page_flush/page_flush.cpp -std=c++17 -g0 -O3  -march=native -DNDEBUG -DSTREAMING=1 -Invml/src/include/ nvml/src/nondebug/libpmem.a nvml/src/nondebug/libpmemblk.a -lpthread -lndctl -ldaxctl
DIRTY_CL_COUNT=16
for THREAD_COUNT in `seq 1 30`; do
  ./a.out ${PAGE_COUNT} ${DIRTY_CL_COUNT} ${THREAD_COUNT} ${REPETITIONS} ${PMEM_PATH}/file | tee -a results/page_flush_threads_16cls.txt
done

# Experiment 3: 7 thread, _x_ dirty cls, streaming
echo "" > results/page_flush_cls_t7.txt
clang++ page_flush/page_flush.cpp -std=c++17 -g0 -O3  -march=native -DNDEBUG -DSTREAMING=1 -Invml/src/include/ nvml/src/nondebug/libpmem.a nvml/src/nondebug/libpmemblk.a -lpthread -lndctl -ldaxctl
THREAD_COUNT=7
for DIRTY_CL_COUNT in `seq 4 4 256`; do
  ./a.out ${PAGE_COUNT} ${DIRTY_CL_COUNT} ${THREAD_COUNT} ${REPETITIONS} ${PMEM_PATH}/file | tee -a results/page_flush_cls_t7.txt
done
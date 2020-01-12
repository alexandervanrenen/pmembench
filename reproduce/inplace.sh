./reproduce/validate.sh || exit -1

# Reproduces data for figure 4
# Write latency

COMPILE="clang++ -g0 -O3 -DNDEBUG=1 -march=native -std=c++17 inplace/bench.cpp -Invml/src/include/ nvml/src/nondebug/libpmem.a nvml/src/nondebug/libpmemlog.a -lpthread -lndctl -ldaxctl"

for ENTRY_SIZE in `seq 16 16 256`; do
  ${COMPILE} -DENTRY_SIZE=${ENTRY_SIZE} || exit -1
  ./a.out 60e6 60e6 /mnt/pmem0/renen
done

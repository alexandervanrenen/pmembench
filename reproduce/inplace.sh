./reproduce/validate.sh || exit -1

# Reproduces data for figure 4
# Write latency

COMPILE="clang++ -g0 -O3 -DNDEBUG=1 -march=native -std=c++17 inplace/bench.cpp -Invml/src/include/ nvml/src/nondebug/libpmem.a nvml/src/nondebug/libpmemlog.a -lpthread -lndctl -ldaxctl"

${COMPILE} -DENTRY_SIZE=16 || exit -1

./a.out 1e6 1e9 /mnt/pmem0/renen
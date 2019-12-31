./reproduce/validate.sh

# Reproduces data for figure 6
# Logging

echo "" > results/logging.txt

clang++ -g0 -O3 -DNEDBUG=1 -march=native -std=c++17 logging/logging.cpp -Invml/src/include/ nvml/src/nondebug/libpmem.a nvml/src/nondebug/libpmemlog.a -lpthread -lndctl -ldaxctl \
&& ./a.out 56 512 1000000000 5 /mnt/pmem0/renen/file_0 0 | tee -a results/logging.txt

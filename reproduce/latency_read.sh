./reproduce/validate.sh || exit -1

# Reproduces data for figure 3
# Read latency

echo "" > results/latency_read.txt

# Read ram
clang++ -g0 -O3 -march=native -DNDEBUG=1 -std=c++14 latency/read_latency.cpp -pthread && ./a.out 1 10e9 1e9 ram ${PMEM_PATH}/file_0 | tee -a results/latency_read.txt

# Read nvm
clang++ -g0 -O3 -march=native -DNDEBUG=1 -std=c++14 latency/read_latency.cpp -pthread && ./a.out 1 10e9 1e9 nvm ${PMEM_PATH}/file_0 | tee -a results/latency_read.txt

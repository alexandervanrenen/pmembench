./reproduce/validate.sh || exit -1

# Reproduces data for figure 4
# Write latency

echo "" > results/latency_write.txt

for type in single sequential random; do
  # Write FLUSH
  clang++ -g0 -O3 -DNDEBUG=1 -march=native -std=c++14 latency/write_latency.cpp -pthread -DFLUSH=1 \
  && ./a.out nvm $type 10e9 ${PMEM_PATH}/file_0 | tee -a results/latency_write.txt

  # Write FLUSH_OPT
  clang++ -g0 -O3 -DNDEBUG=1 -march=native -std=c++14 latency/write_latency.cpp -pthread -DFLUSH_OPT=1 \
  && ./a.out nvm $type 10e9 ${PMEM_PATH}/file_0 | tee -a results/latency_write.txt

  # Write CLWB
  clang++ -g0 -O3 -DNDEBUG=1 -march=native -std=c++14 latency/write_latency.cpp -pthread -DCLWB=1 \
  && ./a.out nvm $type 10e9 ${PMEM_PATH}/file_0 | tee -a results/latency_write.txt

  # Write STREAMING
  clang++ -g0 -O3 -DNDEBUG=1 -march=native -std=c++14 latency/write_latency.cpp -pthread -DSTREAMING=1 \
  && ./a.out nvm $type 10e9 ${PMEM_PATH}/file_0 | tee -a results/latency_write.txt

done
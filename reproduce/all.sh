./reproduce/validate.sh || exit -1

# Reproduces data for all figure
./reproduce/bw_cache_lines.sh
./reproduce/bw_threads.sh
./reproduce/coroutines.sh
./reproduce/inplace.sh
./reproduce/latency_read.sh
./reproduce/latency_write.sh
./reproduce/logging.sh
./reproduce/page_flush.sh

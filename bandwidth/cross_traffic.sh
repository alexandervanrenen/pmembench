
MAX_THREAD_COUNT=24

echo "" > ram_results
echo "" > nvm_results

g++ -g0 -O3 -march=native -std=c++14 bw.cpp -ltbb -pthread -DBLOCK_SIZE=1048576

for RAM in `seq 0 1 $MAX_THREAD_COUNT`; do
   for NVM in `seq 0 1 $MAX_THREAD_COUNT`; do
      echo "RAM="$RAM "NVM="$NVM
      echo "RAM="$RAM "NVM="$NVM >> ram_results
      echo "RAM="$RAM "NVM="$NVM >> nvm_results
      ./a.out 10e9 ${RAM} ram >> ram_results &
      ./a.out 10e9 ${NVM} nvm >> nvm_results &
      wait
   done;
done;

echo "done"

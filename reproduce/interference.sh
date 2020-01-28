./reproduce/validate.sh || exit -1

# Reproduces data for figure 4
# Interference

clang++ -g0 -O3 -DNDEBUG=1 -march=native -std=c++17 interference/interference.cpp -Invml/src/include/ nvml/src/nondebug/libpmem.a nvml/src/nondebug/libpmemlog.a -lpthread -lndctl -ldaxctl || exit -1

# Seqential ram
echo "" > results/interference_seq_ram.txt
./a.out 14  0  0  0  0  0 /mnt/pmem0/renen | tee -a results/interference_seq_ram.txt
./a.out 14  1  0  0  0  0 /mnt/pmem0/renen | tee -a results/interference_seq_ram.txt
./a.out 14  5  0  0  0  0 /mnt/pmem0/renen | tee -a results/interference_seq_ram.txt
./a.out 14 10  0  0  0  0 /mnt/pmem0/renen | tee -a results/interference_seq_ram.txt
./a.out 14  0  1  0  0  0 /mnt/pmem0/renen | tee -a results/interference_seq_ram.txt
./a.out 14  0  5  0  0  0 /mnt/pmem0/renen | tee -a results/interference_seq_ram.txt
./a.out 14  0 10  0  0  0 /mnt/pmem0/renen | tee -a results/interference_seq_ram.txt
./a.out 14  0  0  1  0  0 /mnt/pmem0/renen | tee -a results/interference_seq_ram.txt
./a.out 14  0  0  5  0  0 /mnt/pmem0/renen | tee -a results/interference_seq_ram.txt
./a.out 14  0  0 10  0  0 /mnt/pmem0/renen | tee -a results/interference_seq_ram.txt
./a.out 14  0  0  0  1  0 /mnt/pmem0/renen | tee -a results/interference_seq_ram.txt
./a.out 14  0  0  0  5  0 /mnt/pmem0/renen | tee -a results/interference_seq_ram.txt
./a.out 14  0  0  0 10  0 /mnt/pmem0/renen | tee -a results/interference_seq_ram.txt
./a.out 14  0  0  0  0  1 /mnt/pmem0/renen | tee -a results/interference_seq_ram.txt
./a.out 14  0  0  0  0  5 /mnt/pmem0/renen | tee -a results/interference_seq_ram.txt
./a.out 14  0  0  0  0 10 /mnt/pmem0/renen | tee -a results/interference_seq_ram.txt

# Seqential nvm
echo "" > results/interference_seq_nvm.txt
./a.out  0 14  0  0  0  0 /mnt/pmem0/renen | tee -a results/interference_seq_nvm.txt
./a.out  1 14  0  0  0  0 /mnt/pmem0/renen | tee -a results/interference_seq_nvm.txt
./a.out  5 14  0  0  0  0 /mnt/pmem0/renen | tee -a results/interference_seq_nvm.txt
./a.out 10 14  0  0  0  0 /mnt/pmem0/renen | tee -a results/interference_seq_nvm.txt
./a.out  0 14  1  0  0  0 /mnt/pmem0/renen | tee -a results/interference_seq_nvm.txt
./a.out  0 14  5  0  0  0 /mnt/pmem0/renen | tee -a results/interference_seq_nvm.txt
./a.out  0 14 10  0  0  0 /mnt/pmem0/renen | tee -a results/interference_seq_nvm.txt
./a.out  0 14  0  1  0  0 /mnt/pmem0/renen | tee -a results/interference_seq_nvm.txt
./a.out  0 14  0  5  0  0 /mnt/pmem0/renen | tee -a results/interference_seq_nvm.txt
./a.out  0 14  0 10  0  0 /mnt/pmem0/renen | tee -a results/interference_seq_nvm.txt
./a.out  0 14  0  0  1  0 /mnt/pmem0/renen | tee -a results/interference_seq_nvm.txt
./a.out  0 14  0  0  5  0 /mnt/pmem0/renen | tee -a results/interference_seq_nvm.txt
./a.out  0 14  0  0 10  0 /mnt/pmem0/renen | tee -a results/interference_seq_nvm.txt
./a.out  0 14  0  0  0  1 /mnt/pmem0/renen | tee -a results/interference_seq_nvm.txt
./a.out  0 14  0  0  0  5 /mnt/pmem0/renen | tee -a results/interference_seq_nvm.txt
./a.out  0 14  0  0  0 10 /mnt/pmem0/renen | tee -a results/interference_seq_nvm.txt

# Random ram
echo "" > results/interference_rnd_ram.txt
./a.out  0  0 14  0  0  0 /mnt/pmem0/renen | tee -a results/interference_rnd_ram.txt
./a.out  1  0 14  0  0  0 /mnt/pmem0/renen | tee -a results/interference_rnd_ram.txt
./a.out  5  0 14  0  0  0 /mnt/pmem0/renen | tee -a results/interference_rnd_ram.txt
./a.out 10  0 14  0  0  0 /mnt/pmem0/renen | tee -a results/interference_rnd_ram.txt
./a.out  0  1 14  0  0  0 /mnt/pmem0/renen | tee -a results/interference_rnd_ram.txt
./a.out  0  5 14  0  0  0 /mnt/pmem0/renen | tee -a results/interference_rnd_ram.txt
./a.out  0 10 14  0  0  0 /mnt/pmem0/renen | tee -a results/interference_rnd_ram.txt
./a.out  0  0 14  1  0  0 /mnt/pmem0/renen | tee -a results/interference_rnd_ram.txt
./a.out  0  0 14  5  0  0 /mnt/pmem0/renen | tee -a results/interference_rnd_ram.txt
./a.out  0  0 14 10  0  0 /mnt/pmem0/renen | tee -a results/interference_rnd_ram.txt
./a.out  0  0 14  0  1  0 /mnt/pmem0/renen | tee -a results/interference_rnd_ram.txt
./a.out  0  0 14  0  5  0 /mnt/pmem0/renen | tee -a results/interference_rnd_ram.txt
./a.out  0  0 14  0 10  0 /mnt/pmem0/renen | tee -a results/interference_rnd_ram.txt
./a.out  0  0 14  0  0  1 /mnt/pmem0/renen | tee -a results/interference_rnd_ram.txt
./a.out  0  0 14  0  0  5 /mnt/pmem0/renen | tee -a results/interference_rnd_ram.txt
./a.out  0  0 14  0  0 10 /mnt/pmem0/renen | tee -a results/interference_rnd_ram.txt

# Random nvm
echo "" > results/interference_rnd_nvm.txt
./a.out  0  0  0 14  0  0 /mnt/pmem0/renen | tee -a results/interference_rnd_nvm.txt
./a.out  1  0  0 14  0  0 /mnt/pmem0/renen | tee -a results/interference_rnd_nvm.txt
./a.out  5  0  0 14  0  0 /mnt/pmem0/renen | tee -a results/interference_rnd_nvm.txt
./a.out 10  0  0 14  0  0 /mnt/pmem0/renen | tee -a results/interference_rnd_nvm.txt
./a.out  0  1  0 14  0  0 /mnt/pmem0/renen | tee -a results/interference_rnd_nvm.txt
./a.out  0  5  0 14  0  0 /mnt/pmem0/renen | tee -a results/interference_rnd_nvm.txt
./a.out  0 10  0 14  0  0 /mnt/pmem0/renen | tee -a results/interference_rnd_nvm.txt
./a.out  0  0  1 14  0  0 /mnt/pmem0/renen | tee -a results/interference_rnd_nvm.txt
./a.out  0  0  5 14  0  0 /mnt/pmem0/renen | tee -a results/interference_rnd_nvm.txt
./a.out  0  0 10 14  0  0 /mnt/pmem0/renen | tee -a results/interference_rnd_nvm.txt
./a.out  0  0  0 14  1  0 /mnt/pmem0/renen | tee -a results/interference_rnd_nvm.txt
./a.out  0  0  0 14  5  0 /mnt/pmem0/renen | tee -a results/interference_rnd_nvm.txt
./a.out  0  0  0 14 10  0 /mnt/pmem0/renen | tee -a results/interference_rnd_nvm.txt
./a.out  0  0  0 14  0  1 /mnt/pmem0/renen | tee -a results/interference_rnd_nvm.txt
./a.out  0  0  0 14  0  5 /mnt/pmem0/renen | tee -a results/interference_rnd_nvm.txt
./a.out  0  0  0 14  0 10 /mnt/pmem0/renen | tee -a results/interference_rnd_nvm.txt

## Log nvm
#./a.out  0  0  0  0 10  0 /mnt/pmem0/renen
#./a.out  1  0  0  0 10  0 /mnt/pmem0/renen
#./a.out  5  0  0  0 10  0 /mnt/pmem0/renen
#./a.out 10  0  0  0 10  0 /mnt/pmem0/renen
#./a.out  0  1  0  0 10  0 /mnt/pmem0/renen
#./a.out  0  5  0  0 10  0 /mnt/pmem0/renen
#./a.out  0 10  0  0 10  0 /mnt/pmem0/renen
#./a.out  0  0  1  0 10  0 /mnt/pmem0/renen
#./a.out  0  0  5  0 10  0 /mnt/pmem0/renen
#./a.out  0  0 10  0 10  0 /mnt/pmem0/renen
#./a.out  0  0  0  1 10  0 /mnt/pmem0/renen
#./a.out  0  0  0  5 10  0 /mnt/pmem0/renen
#./a.out  0  0  0 10 10  0 /mnt/pmem0/renen
#./a.out  0  0  0  0 10  1 /mnt/pmem0/renen
#./a.out  0  0  0  0 10  5 /mnt/pmem0/renen
#./a.out  0  0  0  0 10 10 /mnt/pmem0/renen
#
## Page nvm
#./a.out  0  0  0  0  0 10 /mnt/pmem0/renen
#./a.out  1  0  0  0  0 10 /mnt/pmem0/renen
#./a.out  5  0  0  0  0 10 /mnt/pmem0/renen
#./a.out 10  0  0  0  0 10 /mnt/pmem0/renen
#./a.out  0  1  0  0  0 10 /mnt/pmem0/renen
#./a.out  0  5  0  0  0 10 /mnt/pmem0/renen
#./a.out  0 10  0  0  0 10 /mnt/pmem0/renen
#./a.out  0  0  1  0  0 10 /mnt/pmem0/renen
#./a.out  0  0  5  0  0 10 /mnt/pmem0/renen
#./a.out  0  0 10  0  0 10 /mnt/pmem0/renen
#./a.out  0  0  0  1  0 10 /mnt/pmem0/renen
#./a.out  0  0  0  5  0 10 /mnt/pmem0/renen
#./a.out  0  0  0 10  0 10 /mnt/pmem0/renen
#./a.out  0  0  0  0  1 10 /mnt/pmem0/renen
#./a.out  0  0  0  0  5 10 /mnt/pmem0/renen
#./a.out  0  0  0  0 10 10 /mnt/pmem0/renen

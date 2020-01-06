#include <atomic>
#include <iostream>
#include <algorithm>
#include <pthread.h>
#include <cstdint>
#include <immintrin.h>
#include <chrono>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <vector>
#include <unistd.h>

using namespace std;

inline void Clwb(void *addr)
{
#ifdef NOCLWB
   (void) addr;
#else
   asm volatile(".byte 0x66; xsaveopt %0" : "+m" (*(volatile char *) addr));
#endif
}

uint64_t rdtsc()
{
   uint32_t hi, lo;
   __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
   return static_cast<uint64_t>(lo) | (static_cast<uint64_t>(hi) << 32);
}

inline void SFence()
{
   _mm_mfence();
}

#include <sys/time.h>

static inline double gettime(void)
{
   struct timeval now_tv;
   gettimeofday(&now_tv, NULL);
   return ((double) now_tv.tv_sec) + ((double) now_tv.tv_usec) / 1000000.0;
}

uint64_t COUNT; // In number of uint64_t
uint64_t SIZE; // In byte
uint64_t *v;
atomic<bool> go(0);
const char *PATH;
bool USE_RAM;

static void *readThread(void *arg)
{
   while (!go);
   uintptr_t threadNum = reinterpret_cast<uintptr_t>(arg);

   uint64_t x = 0;
   auto start = chrono::high_resolution_clock::now();
   for (uint64_t i = 0; i<COUNT; i++) {
      uint64_t next = v[x];
      v[x] = x;
      Clwb(&v[x]);
      SFence();
      x = next;
   }
   auto end = chrono::high_resolution_clock::now();
   uint64_t ns = chrono::duration_cast<chrono::nanoseconds>(end - start).count();

   cout << threadNum << ": " << (ns / (double) COUNT) << " ns/rep (result=" << x << ")" << endl;
   return (void *) (ns / COUNT);
}

class FastRandom {
public:
   explicit FastRandom(uint64_t seed = 2305843008139952128ull) // The 8th perfect number found 1772 by Euler with <3
           : seed(seed) {}

   uint64_t Next()
   {
      seed ^= (seed << 13);
      seed ^= (seed >> 15);
      return (seed ^= (seed << 5));
   }

   uint64_t seed;
};

uint64_t *CreateRandomCycle(bool ram)
{
   uint64_t *helper;
   uint64_t *result;

   {
      auto start = chrono::high_resolution_clock::now();
      cout << "init " << flush;
      helper = new uint64_t[COUNT];
      for (uint64_t i = 0; i<COUNT;) {
         uint64_t limit = min(i + COUNT / 10, COUNT);
         for (; i<limit; i++) {
            helper[i] = i;
         }
         cout << "." << flush;
      }
      auto end = chrono::high_resolution_clock::now();
      double s = chrono::duration_cast<chrono::seconds>(end - start).count();
      cout << "(" << s << ")" << endl;
   }

   {
      auto start = chrono::high_resolution_clock::now();
      cout << "shuffle " << flush;
      FastRandom ranny;
      for (uint64_t i = 0; i<COUNT;) {
         uint64_t limit = min(i + COUNT / 10, COUNT);
         for (; i<limit; i++) {
            uint64_t pos = (ranny.Next() % (COUNT - i)) + i;
            swap(helper[i], helper[pos]);
         }
         cout << "." << flush;
      }
      auto end = chrono::high_resolution_clock::now();
      double s = chrono::duration_cast<chrono::seconds>(end - start).count();
      cout << "(" << s << ")" << endl;
   }

   {
      auto start = chrono::high_resolution_clock::now();
      cout << "cycle " << flush;
      if (ram) {
         result = new uint64_t[COUNT];
      } else {
         int fd = open(PATH, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
         int td = ftruncate(fd, COUNT * 8);
         if (fd<0 || td<0) {
            cout << "unable to create file" << endl;
            exit(-1);
         }
         result = (uint64_t *) mmap(nullptr, COUNT * 8, PROT_WRITE, MAP_SHARED, fd, 0);
      }
      for (uint64_t i = 0; i<COUNT;) {
         uint64_t limit = min(i + COUNT / 10, COUNT);
         for (; i<limit; i++) {
            result[helper[i]] = helper[(i + 1) % COUNT];
         }
         cout << "." << flush;
      }
      auto end = chrono::high_resolution_clock::now();
      double s = chrono::duration_cast<chrono::seconds>(end - start).count();
      cout << "(" << s << ")" << endl;
   }

   return result;
}

// clang++ -g0 -O3 -march=native -std=c++14 read_latency.cpp -pthread && ./a.out 1 1e9 1e9 ram /mnt/pmem0/renen/file_0
int main(int argc, char **argv)
{
   if (argc != 5) {
      cout << "usage: " << argv[0] << " thread_count datasize(byte) (nvm|ram) path" << endl;
      throw;
   }

   unsigned threadCount = atoi(argv[1]);
   SIZE = atof(argv[2]);
   COUNT = SIZE / 8;
   USE_RAM = argv[3][0] == 'r';
   PATH = argv[4];

   cout << "Config: thread_count=" << threadCount << " use_ram=" << USE_RAM << " size=" << SIZE << " path=" << PATH << endl;

   v = CreateRandomCycle(USE_RAM);
   cout << "starting .." << endl;

   pthread_t threads[threadCount];
   for (unsigned i = 0; i<threadCount; i++) {
      pthread_create(&threads[i], NULL, readThread, reinterpret_cast<void *>(i));
   }

   uint64_t times[threadCount];
   auto start = chrono::high_resolution_clock::now();
   go = 1;
   for (unsigned i = 0; i<threadCount; i++) {
      void *ret;
      pthread_join(threads[i], &ret);
      times[i] = (uint64_t) ret;
   }
   auto end = chrono::high_resolution_clock::now();

   for (uint64_t i = 0; i<COUNT; i++) {
      if (i != v[i]) {
         cout << "at position " << i << " we have a " << v[i] << endl;
         throw;
      }
   }

   double ns = chrono::duration_cast<chrono::nanoseconds>(end - start).count();
   double gb = ((threadCount * 64 * COUNT) / (1000.0 * 1000.0 * 1000.0));
   double gbs = gb / ns * 1e9;
   double latency = ns / COUNT;

   // @formatter:off
   cout << "res:"
        << " thread_count=" << threadCount
        << " use_ram=" << USE_RAM
        << " size=" << SIZE
        << " throughput(GB/s) " << gbs
        << " latency(ns) " << latency
        << endl;
   // @formatter:on

   return 0;
}

#include <sys/time.h>
#include <iostream>
#include <atomic>
#include <iostream>
#include <vector>
#include <cstdint>
#include <immintrin.h>
#include <chrono>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <thread>
#include <unistd.h>
#include <xmmintrin.h>
#include <fcntl.h>
#include <sstream>
#include <cstring>
#include <cassert>
#include <sys/stat.h>
#include <algorithm>

using namespace std;

#define _mm_clflush(addr)\
    asm volatile("clflush %0" : "+m" (*(volatile char *)addr));
#define _mm_clflushopt(addr)\
    asm volatile(".byte 0x66; clflush %0" : "+m" (*(volatile char *)addr));
#define _mm_clwb(addr)\
    asm volatile(".byte 0x66; xsaveopt %0" : "+m" (*(volatile char *)addr));
#define _mm_pcommit()\
    asm volatile(".byte 0x66, 0x0f, 0xae, 0xf8");

enum struct Type {
   RANDOM_CL,
   SEQUENTIAL_CL,
   SAME_CL
};

Type GetClWriteType(const string &str)
{
   if (str == "single")
      return Type::SAME_CL;
   if (str == "sequential")
      return Type::SEQUENTIAL_CL;
   if (str == "random") {
      return Type::RANDOM_CL;
   }
   cout << "unknown benchmark type: '" << str << "'" << endl;
   throw;
}

std::string GetStringFromType(Type benchmark_type)
{
   switch (benchmark_type) {
      case Type::RANDOM_CL: {
         return "RANDOM_CL";
      }
      case Type::SEQUENTIAL_CL: {
         return "SEQUENTIAL_CL";
      }
      case Type::SAME_CL: {
         return "SAME_CL";
      }
   }
   cout << "unkown type" << endl;
   throw;
}

// clang++ -g0 -O3 -march=native -std=c++14 write_latency.cpp -pthread -DFLUSH=1 && ./a.out ram sequential 1e9 /mnt/pmem0/renen/file_0
int main(int argc, char **argv)
{
   if (argc != 5) {
      cout << "usage: " << argv[0] << " (nvm|ram) (single|sequential|random) size(byte) path" << endl;
      throw;
   }
   bool use_flush = false;
   bool use_flush_opt = false;
   bool use_clwb = false;
   bool use_streaming = false;
   int technique_count = 0;
#ifdef FLUSH
   use_flush = true;
   technique_count++;
#endif
#ifdef FLUSH_OPT
   use_flush_opt = true;
   technique_count++;
#endif
#ifdef CLWB
   use_clwb= true;
   technique_count++;
#endif
#ifdef STREAMING
   use_streaming = true;
   technique_count++;
#endif
   if (technique_count != 1) {
      cout << "Need to specify exactly one flush technique" << endl;
      throw;
   }
   const bool use_ram = argv[1][0] == 'r';
   const Type benchmark_type = GetClWriteType(argv[2]);
   const uint64_t size = atof(argv[3]);
   const string path = argv[4];

   cout << "use_flush: " << (use_flush ? "yes" : "no") << endl;
   cout << "use_flush_opt: " << (use_flush_opt ? "yes" : "no") << endl;
   cout << "use_clwb: " << (use_clwb ? "yes" : "no") << endl;
   cout << "use_streaming: " << (use_streaming ? "yes" : "no") << endl;
   cout << "use_ram: " << (use_ram ? "yes" : "no") << endl;
   cout << "benchmark_type: " << GetStringFromType(benchmark_type) << endl;
   cout << "size: " << size << endl;

   uint8_t *keys;
   if (use_ram) {
      keys = new uint8_t[size + 64];
      while (((uint64_t) keys) % 64 != 0) // Align to 64 byte ;p
         keys++;
   } else {
      int fd = open(path.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
      int td = ftruncate(fd, size);
      if (fd<0 || td<0) {
         cout << "unable to create file" << endl;
         exit(-1);
      }
      keys = (uint8_t *) mmap(nullptr, size, PROT_WRITE, MAP_SHARED, fd, 0);
   }

   uint8_t write_data[64] = {0xaa};
   __m512i write_data_vec = _mm512_stream_load_si512(write_data);

   vector<int64_t> write_offsets(size / 64);
   switch (benchmark_type) {
      case Type::RANDOM_CL: {
         for (uint64_t i = 0; i<write_offsets.size(); i++) {
            write_offsets[i] = i * 64;
         }
         random_shuffle(write_offsets.begin(), write_offsets.end());
         break;
      }
      case Type::SEQUENTIAL_CL: {
         for (uint64_t i = 0; i<write_offsets.size(); i++) {
            write_offsets[i] = i * 64;
         }
         break;
      }
      case Type::SAME_CL: {
         for (uint64_t i = 0; i<write_offsets.size(); i++) {
            write_offsets[i] = 0;
         }
         break;
      }
   }

   uint64_t iteration_count = size / 64;
   auto begin = chrono::high_resolution_clock::now();
   for (uint64_t i = 0; i<iteration_count; i++) {
      uint64_t *write_ptr = (uint64_t *) (keys + write_offsets[i]);

#ifdef FLUSH
      *write_ptr = i;
      _mm_clflush(write_ptr);
      _mm_sfence();
#endif

#ifdef FLUSH_OPT
      *write_ptr = i;
      _mm_clflushopt(write_ptr);
      _mm_sfence();
#endif

#ifdef CLWB
      *write_ptr = i;
      _mm_clwb(write_ptr);
      _mm_sfence();
#endif

#ifdef STREAMING
      _mm512_stream_si512 ((__m512i*)(write_ptr), write_data_vec);
      _mm_sfence();
#endif
   }
   auto end = chrono::high_resolution_clock::now();

   uint64_t nano_seconds = chrono::duration_cast<chrono::nanoseconds>(end - begin).count();
   uint64_t latency = nano_seconds / iteration_count;

   // @formatter:off
   std::cout << "res "
             << " use_ram: " << use_ram
             << " size: " << size
             << " benchmark_type: " << GetStringFromType(benchmark_type)
             << " use_flush: " << use_flush
             << " use_flush_opt: " << use_flush_opt
             << " use_clwb: " << use_clwb
             << " use_streaming: " << use_streaming
             << " latency(ns): " << latency << std::endl;
   // @formatter:on

   return 0;
}


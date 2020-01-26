#pragma once
// -------------------------------------------------------------------------------------
#include "Common.hpp"
#include "NonVolatileMemory.hpp"
#include <atomic>
#include <thread>
#include <immintrin.h>
#include <array>
#include <bitset>
#include <cstring>
#include <unordered_map>
#include <iostream>
// -------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------
class RandomReader : public Worker {
   string nvm_file;
   ub8 byte_count;
   bool is_ram;
   ub8 *data;
   unique_ptr<NonVolatileMemory> nvm;
   vector<double> nano_seconds;
   ub8 sum;

public:
   RandomReader(const string &nvm_file, ub8 byte_count, bool is_ram, ub4 tid, string config)
           : Worker(tid, config)
             , byte_count(byte_count)
             , nvm_file(nvm_file)
             , is_ram(is_ram)
   {
      assert(byte_count % 8 == 0);
      if (byte_count % 8 != 0) {
         throw "byte_count % 8";
      }
   }

   void Run()
   {
      Setup();
      ready = true;
      while (!run) {
         this_thread::yield();
      }

      while (!stop) {
         auto begin = chrono::high_resolution_clock::now();
         sum += DoOneRun();
         auto end = chrono::high_resolution_clock::now();
         nano_seconds.push_back(chrono::duration_cast<chrono::nanoseconds>(end - begin).count());
         performed_iteration_count++;
      }
   }

   void PrintResultOfLastIteration(ub4 iteration)
   {
      //      if (!stop || iteration>=performed_iteration_count) {
      //         throw;
      //      }
      double ns = nano_seconds[iteration];
      //@formatter:off
      cout << "RES " << (is_ram ? "rnd_ram_reader " : "rnd_nvm_reader ")  << config
           << " tid: " << tid
           << " iterations: " << iteration << "/" << performed_iteration_count
           << " sum: " << sum
           << " perf(reads/s): " << ub8((byte_count / 8) / (ns / 1e9)) << endl;
      //@formatter:on
   }

   RandomReader(const RandomReader &) = delete;
   RandomReader &operator=(const RandomReader &) = delete;

private:
   void Setup()
   {
      // Alloc
      if (is_ram) {
         data = (ub8 *) malloc(byte_count + 64);
      } else {
         nvm = make_unique<NonVolatileMemory>(nvm_file, byte_count + 64);
         data = (ub8 *) nvm->Data();
      }

      while ((ub8) data % 8 != 0) {
         data++;
      }
      assert((ub8) data % 8 == 0);

      Random ranny;
      for (uint64_t i = 0; i<(byte_count / 8); i++) {
         data[i] = ranny.Rand();
      }
   }

   ub8 DoOneRun()
   {
      Random ranny;
      ub8 sum = 0;
      for (ub8 i = 0; i<byte_count / 8; i++) {
         ub8 offset = ranny.Rand() % (byte_count / 8);
         sum += data[offset];
      }
      return sum;
   }
};
// -------------------------------------------------------------------------------------

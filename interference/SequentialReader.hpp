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
class SequentialReader {
   string nvm_file;
   ub8 byte_count;
   bool is_ram;
   ub8 *data;
   ub8 expected_sum = 0;
   unique_ptr<NonVolatileMemory> nvm;

public:
   atomic<bool> run;
   atomic<bool> ready;
   atomic<bool> stop;

   SequentialReader(const string &nvm_file, ub8 byte_count, bool is_ram)
           : byte_count(byte_count)
             , nvm_file(nvm_file)
             , is_ram(is_ram)
             , run(false)
             , ready(false)
             , stop(false)
   {
      assert(byte_count % 8);
      if (byte_count % 8 != 0) {
         throw "byte_count % 8";
      }
   }

   void Run(ub4 thread_id)
   {
      Setup();
      ready = true;
      while (!run) {
         this_thread::yield();
      }

      while (!stop) {
         auto begin = chrono::high_resolution_clock::now();
         DoOneRun();
         auto end = chrono::high_resolution_clock::now();
         double ns = chrono::duration_cast<chrono::nanoseconds>(end - begin).count();

         unique_lock<mutex> l(global_io_mutex);
         cout << "RES seq_reader tid= " << thread_id << " perf(gb/s): " << (byte_count / ns) << endl;
      }
   }

   SequentialReader(const SequentialReader &) = delete;
   SequentialReader &operator=(const SequentialReader &) = delete;

private:
   void Setup()
   {
      Random ranny;
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

      for (ub8 i = 0; i<byte_count / 8; i++) {
         ub8 val = ranny.Rand();
         data[i] = val;
         expected_sum += val;
      }
   }

   void DoOneRun()
   {
      ub8 sum = 0;
      for (ub8 i = 0; i<byte_count / 8; i++) {
         sum += data[i];
      }
      if (sum != expected_sum) {
         cout << "sequential reader thread broke" << endl;
         throw;
      }
   }
};
// -------------------------------------------------------------------------------------

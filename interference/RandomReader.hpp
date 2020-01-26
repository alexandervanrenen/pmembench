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
class RandomReader {
   string nvm_file;
   ub8 byte_count;
   bool is_ram;
   ub8 *data;
   unique_ptr<NonVolatileMemory> nvm;
   vector<ub8> operations;

public:
   atomic<bool> run;
   atomic<bool> ready;
   atomic<bool> stop;

   RandomReader(const string &nvm_file, ub8 byte_count, bool is_ram)
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
         ub8 res = DoOneRun();
         auto end = chrono::high_resolution_clock::now();
         double ns = chrono::duration_cast<chrono::nanoseconds>(end - begin).count();

         cout << thread_id << " -> " << " (res=" << res << ") time: " << (byte_count / ns) << endl;
      }
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
      operations.resize((byte_count / 8));
      for (uint64_t i = 0; i<(byte_count / 8); i++) {
         operations[i] = ranny.Rand() % (byte_count / 8);
      }
   }

   ub8 DoOneRun()
   {
      Random ranny;
      ub8 sum = 0;
      for (ub8 i = 0; i<byte_count / 8; i++) {
         sum += data[operations[i]];
      }
      return sum;
   }
};
// -------------------------------------------------------------------------------------

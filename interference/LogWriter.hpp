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
namespace log_utils {
// -------------------------------------------------------------------------------------
inline ub8 alex_PopCount(ub8 value)
{
   return _mm_popcnt_u64(value);
}
// -------------------------------------------------------------------------------------
inline void alex_FlushClToNvm(void *dest, void *src)
{
   assert(((ub8) dest) % 64 == 0);
   assert(((ub8) src) % 64 == 0);
   __m512i reg = _mm512_load_si512(src);
   _mm512_stream_si512((__m512i *) dest, reg);
}
// -------------------------------------------------------------------------------------
ub4 FastPopCount512(const ub1 *ptr)
{
   ub4 res = 0;
   for (ub4 i = 0; i<64; i += 8) {
      res += alex_PopCount(*(ub8 *) (&ptr[i]));
   }
   return res;
}
// -------------------------------------------------------------------------------------
void FastCopy512(ub1 *dest, const ub1 *src)
{
   assert(((ub8) dest) % 64 == 0);
   memcpy(dest, src, 64);
}
// -------------------------------------------------------------------------------------
}
// -------------------------------------------------------------------------------------
struct LogWriterZeroBlocked {

   struct Entry {
      ub4 payload_size; // header
      ub4 bit_count; // header
      ub1 data[];
   };

   struct File {
      // Header
      ub1 padding[constants::kCacheLineByteCount];

      // Log data
      ub1 data[];
   };
   static_assert(sizeof(File) == 64, "");

   NonVolatileMemory &nvm;
   File &file; // == nvm
   ub8 next_free;

   LogWriterZeroBlocked(NonVolatileMemory &nvm)
           : nvm(nvm)
             , file(*reinterpret_cast<File *>(nvm.Data()))
   {
      Reset();
   }

   void Reset()
   {
      next_free = 0;
   }

   ub8 AddLogEntry(const Entry &entry)
   {
      ub4 size = entry.payload_size + 8;

      assert(next_free % 8 == 0);
      assert(entry.bit_count == 0);
      assert(entry.payload_size % 8 == 0);
      assert(next_free + size<nvm.GetByteCount());

      assert(size>=64);

      const ub1 *ram_begin = reinterpret_cast<const ub1 *>(&entry);
      ub1 *nvm_begin = reinterpret_cast<ub1 *>(nvm.Data() + next_free);

      // Copy first cache line (and do not flush)
      ub4 pop_cnt = 0;
      log_utils::FastCopy512(nvm_begin, ram_begin);
      pop_cnt += log_utils::FastPopCount512(ram_begin);

      // Copy remaining full cache lines (and flush)
      ub4 pos = 64;
      for (; pos + 63<size; pos += 64) {
         log_utils::FastCopy512(nvm_begin + pos, ram_begin + pos);
         pop_cnt += log_utils::FastPopCount512(ram_begin + pos);
         alex_WriteBack(nvm_begin + pos);
      }

      // Copy remaining bytes (and flush)
      if (pos<size) {
         for (; pos<size; pos += 8) {
            *(ub8 *) (nvm_begin + pos) = *(ub8 *) (ram_begin + pos);
            pop_cnt += log_utils::alex_PopCount(*(ub8 *) (ram_begin + pos));
         }
         alex_WriteBack(nvm_begin + pos - 8);
      }

      // Write pop count and flush first cache line
      reinterpret_cast<Entry *>(nvm_begin)->bit_count = pop_cnt;
      alex_WriteBack(nvm_begin);
      alex_SFence();

      // Advance and done
      next_free += entry.payload_size + 8;
      next_free = (next_free + 63) & ~63ull;
      assert(next_free % 64 == 0);
      return next_free;
   }

   ub8 GetWrittenByteCount() const
   {
      return next_free + sizeof(File);
   }

   static vector<Entry *> CreateRandomEntries(vector <ub1> &memory, ub4 min_size, ub4 max_size, ub8 log_payload_size, Random &ranny)
   {
      ub8 current = 0;
      ub8 used_size = 0;

      vector<Entry *>entries;
      while (used_size<log_payload_size) {
         ub8 payload_size = ((ranny.Rand() % (max_size - min_size + 1)) + min_size) * 8;
         assert(current + payload_size + 8<memory.size());
         Entry *entry = (Entry *) &memory[current];
         entry->payload_size = payload_size;
         entry->bit_count = 0;
         entries.push_back(entry);
         current += payload_size + 8;
         used_size += payload_size;
      }
      return entries;
   }
};
// -------------------------------------------------------------------------------------
class LogWriter : public Worker {
   unique_ptr <LogWriterZeroBlocked> wal;
   unique_ptr <NonVolatileMemory> nvm;
   vector <ub1> memory;
   vector<LogWriterZeroBlocked::Entry *> entries;

   ub8 byte_count;
   string nvm_file;

   const ub4 ENTRY_SIZE = 104;

   vector<double> nano_seconds;

public:
   LogWriter(const string &nvm_file, ub8 byte_count, ub4 tid, string config)
           : Worker(tid, config)
             , byte_count(byte_count)
             , nvm_file(nvm_file)
   {
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
         DoOneRun();
         auto end = chrono::high_resolution_clock::now();
         nano_seconds.push_back(chrono::duration_cast<chrono::nanoseconds>(end - begin).count());
         performed_iteration_count++;
      }
   }

   void PrintResultOfLastIteration(ub4 iteration)
   {
      if (!stop || iteration>=performed_iteration_count) {
         throw;
      }
      double ns = nano_seconds[iteration];
      //@formatter:off
      cout << "RES log_writer " << config
           << " tid: " << tid
           << " iterations: " << iteration << "/" << performed_iteration_count
           << " perf(logs/s): " << ub8(entries.size() / (ns / 1e9)) << endl;
      //@formatter:on
   }

   LogWriter(const LogWriter &) = delete;
   LogWriter &operator=(const LogWriter &) = delete;

private:
   void Setup()
   {
      Random ranny;
      nvm = make_unique<NonVolatileMemory>(nvm_file, byte_count * 2); // * 2 to not overflow because of header
      wal = make_unique<LogWriterZeroBlocked>(*nvm);

      memory = RandomizedMemory(byte_count * 2, ranny);
      entries = LogWriterZeroBlocked::CreateRandomEntries(memory, ENTRY_SIZE / 8, ENTRY_SIZE / 8, byte_count, ranny);
   }

   void DoOneRun()
   {
      wal->Reset();
      for (LogWriterZeroBlocked::Entry *entry : entries) {
         wal->AddLogEntry(*entry);
      }
   }

   vector <ub1> RandomizedMemory(ub8 size, Random &ranny)
   {
      vector <ub1> memory(size);
      for (ub8 i = 0; i<size; i += 8) {
         ((ub8 &) memory[i]) = ranny.Rand();
      }
      return memory;
   }
};
// -------------------------------------------------------------------------------------

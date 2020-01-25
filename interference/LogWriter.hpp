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
}
// -------------------------------------------------------------------------------------
struct LogWriterZeroCached {

   struct Entry {
      ub8 payload_size; // header
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
   ub8 cl_pos;
   ub8 active_cl_mem[16];
   ub8 *active_cl;
   ub8 log_read_offset;

   LogWriterZeroCached(NonVolatileMemory &nvm)
           : nvm(nvm)
             , file(*reinterpret_cast<File *>(nvm.Data()))
   {
      Reset();
   }

   void Reset()
   {
      next_free = 0;
      cl_pos = 0;
      log_read_offset = 0;

      active_cl = active_cl_mem;
      while ((ub8) active_cl % 64 != 0) {
         active_cl++;
      }
      assert((ub8) active_cl % 64 == 0);
      memset((ub1 *) active_cl, 0, 64);
   }

   ub8 AddLogEntry(const Entry &entry)
   {
      ub4 size = entry.payload_size + 8;
      ub4 blks = size / 8;

      assert(next_free % 8 == 0);
      assert(entry.payload_size % 8 == 0);
      assert(next_free + size<nvm.GetByteCount());

      ub4 pop_cnt = 0;
      const ub8 *ram_begin = reinterpret_cast<const ub8 *>(&entry);
      ub1 *nvm_begin = reinterpret_cast<ub1 *>(file.data + (next_free & ~63ull));

      // Head
      ub4 pos = 0;
      for (; pos<blks && cl_pos != 0; pos++) {
         active_cl[cl_pos] = ram_begin[pos];
         pop_cnt += log_utils::alex_PopCount(ram_begin[pos]);
         cl_pos++;

         if (cl_pos % 8 == 0) {
            log_utils::alex_FlushClToNvm(nvm_begin, (ub1 *) active_cl);
            memset((ub1 *) active_cl, 0, 64);
            cl_pos = 0;
            nvm_begin += 64;
         }
      }

      // Copy all 8-byte-blocks of log-entry via cl-buffer to nvm
      for (; pos + 7<blks; pos += 8) {
         active_cl[0] = ram_begin[pos + 0];
         pop_cnt += log_utils::alex_PopCount(ram_begin[pos + 0]);

         active_cl[1] = ram_begin[pos + 1];
         pop_cnt += log_utils::alex_PopCount(ram_begin[pos + 1]);

         active_cl[2] = ram_begin[pos + 2];
         pop_cnt += log_utils::alex_PopCount(ram_begin[pos + 2]);

         active_cl[3] = ram_begin[pos + 3];
         pop_cnt += log_utils::alex_PopCount(ram_begin[pos + 3]);

         active_cl[4] = ram_begin[pos + 4];
         pop_cnt += log_utils::alex_PopCount(ram_begin[pos + 4]);

         active_cl[5] = ram_begin[pos + 5];
         pop_cnt += log_utils::alex_PopCount(ram_begin[pos + 5]);

         active_cl[6] = ram_begin[pos + 6];
         pop_cnt += log_utils::alex_PopCount(ram_begin[pos + 6]);

         active_cl[7] = ram_begin[pos + 7];
         pop_cnt += log_utils::alex_PopCount(ram_begin[pos + 7]);

         log_utils::alex_FlushClToNvm(nvm_begin, (ub1 *) active_cl);
         memset((ub1 *) active_cl, 0, 64);
         cl_pos = 0;
         nvm_begin += 64;
      }

      // Tail
      for (; pos<blks; pos++) {
         active_cl[cl_pos] = ram_begin[pos];
         pop_cnt += log_utils::alex_PopCount(ram_begin[pos]);
         cl_pos++;
      }

      //      cout << "writing pop_cnt to " << (ub8(nvm_begin) - ub8(file.data)) + cl_pos << endl;
      active_cl[cl_pos] = pop_cnt;
      cl_pos++;
      log_utils::alex_FlushClToNvm(nvm_begin, (ub1 *) active_cl);
      alex_SFence();

      if (cl_pos % 8 == 0) {
         memset((ub1 *) active_cl, 0, 64);
         cl_pos = 0;
         nvm_begin += 64;
      }

      // Advance and done
      next_free += entry.payload_size + 16;
      return next_free;
   }

   unique_ptr <Entry> GetNextLogEntry() // Read code is only to verify correctnes
   {
      if (log_read_offset == next_free) {
         return nullptr;
      }

      // Read length
      ub8 len = *(ub8 *) &file.data[log_read_offset];
      log_read_offset += 8;
      ub8 pop_cnt = 0;
      pop_cnt += log_utils::alex_PopCount(len);

      // Read data
      vector <ub8> result(len);
      for (ub4 pos = 0; pos<len; pos += 8) {
         result[pos / 8] = *(ub8 *) (file.data + log_read_offset);
         log_read_offset += 8;
         pop_cnt += log_utils::alex_PopCount(result[pos / 8]);
      }

      // Read pop cnt
      ub8 read_pop_cnt = *(ub8 *) (file.data + log_read_offset);
      log_read_offset += 8;
      if (read_pop_cnt != pop_cnt) {
         cout << "read_pop_cnt does not match !! " << read_pop_cnt << " vs " << pop_cnt << endl;
         throw;
      }

      Entry *entry = new(malloc(sizeof(Entry) + result.size())) Entry();
      entry->payload_size = result.size();
      memcpy(entry->data, (ub1 *) result.data(), result.size());
      return unique_ptr<Entry>(entry);
   }

   ub8 GetWrittenByteCount() const
   {
      return next_free + sizeof(File);
   }

   // Omg, the naming here is horrible, sorry ;)
   // min_size/max_size is the number of 8 byte values in a log entry
   // log_payload_size is the total size when you sum up all log entries
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
         entries.push_back(entry);
         current += payload_size + 8;
         used_size += payload_size;
      }
      return entries;
   }
};
// -------------------------------------------------------------------------------------
class LogWriter {
   unique_ptr <LogWriterZeroCached> wal;
   unique_ptr <NonVolatileMemory> nvm;
   vector <ub1> memory;
   vector<LogWriterZeroCached::Entry *> entries;

   ub8 byte_count;
   string nvm_file;

   const ub4 ENTRY_SIZE = 100;

public:
   atomic<bool> run;
   atomic<bool> ready;
   atomic<bool> stop;

   LogWriter(const string &nvm_file, ub8 byte_count)
           : byte_count(byte_count)
             , nvm_file(nvm_file)
             , run(false)
             , ready(false)
             , stop(false)
   {
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

         double latency = ns * 1.0 / entries.size();
         cout << thread_id << " -> " << latency << endl;
      }
   }

   LogWriter(const LogWriter &) = delete;
   LogWriter &operator=(const LogWriter &) = delete;

private:
   void Setup()
   {
      Random ranny;
      nvm = make_unique<NonVolatileMemory>(nvm_file, byte_count * 2); // * 2 to not overflow because of header
      wal = make_unique<LogWriterZeroCached>(*nvm);

      memory = RandomizedMemory(byte_count * 2, ranny);
      entries = LogWriterZeroCached::CreateRandomEntries(memory, ENTRY_SIZE / 8, ENTRY_SIZE / 8, byte_count, ranny);
   }

   void DoOneRun()
   {
      wal->Reset();
      for (LogWriterZeroCached::Entry *entry : entries) {
         wal->AddLogEntry(*entry);
      }
   }

   vector<ub1> RandomizedMemory(ub8 size, Random &ranny)
   {
      vector<ub1> memory(size);
      for (ub8 i = 0; i<size; i += 8) {
         ((ub8 &) memory[i]) = ranny.Rand();
      }
      return memory;
   }
};
// -------------------------------------------------------------------------------------

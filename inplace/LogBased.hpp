#include "Common.hpp"
#include "NonVolatileMemory.hpp"
#include <memory>
// -------------------------------------------------------------------------------------
struct LogWriterZeroCached {

   struct Entry {
      uint64_t payload_size; // header
      uint64_t entry_id; // header
      uint8_t data[];
   };

   struct File {
      // Header
      uint8_t padding[constants::kCacheLineByteCount];

      // Log data
      uint8_t data[];
   };
   static_assert(sizeof(File) == 64, "");

   NonVolatileMemory &nvm;
   File &file; // == nvm
   uint64_t next_free;
   uint64_t cl_pos;
   uint64_t active_cl_mem[16];
   uint64_t *active_cl;
   uint64_t log_read_offset;

   LogWriterZeroCached(NonVolatileMemory &nvm)
           : nvm(nvm)
             , file(*reinterpret_cast<File *>(nvm.Data()))
   {
      next_free = 0;
      cl_pos = 0;
      log_read_offset = 0;

      active_cl = active_cl_mem;
      while ((uint64_t) active_cl % 64 != 0) {
         active_cl++;
      }
      assert((uint64_t) active_cl % 64 == 0);
      memset((uint8_t *) active_cl, 0, 64);
   }

   uint64_t AddLogEntry(const Entry &entry)
   {
      uint32_t size = entry.payload_size + 8;
      uint32_t blks = size / 8;

      assert(next_free % 8 == 0);
      assert(entry.payload_size % 8 == 0);
      assert(next_free + size<nvm.GetByteCount());

      uint32_t pop_cnt = 0;
      const uint64_t *ram_begin = reinterpret_cast<const uint64_t *>(&entry);
      uint8_t *nvm_begin = reinterpret_cast<uint8_t *>(file.data + (next_free & ~63ull));

      // Head
      uint32_t pos = 0;
      for (; pos<blks && cl_pos != 0; pos++) {
         active_cl[cl_pos] = ram_begin[pos];
         pop_cnt += alex_PopCount(ram_begin[pos]);
         cl_pos++;

         if (cl_pos % 8 == 0) {
            alex_FlushClToNvm(nvm_begin, (uint8_t *) active_cl);
            memset((uint8_t *) active_cl, 0, 64);
            cl_pos = 0;
            nvm_begin += 64;
         }
      }

      // Copy all 8-byte-blocks of log-entry via cl-buffer to nvm
      for (; pos + 7<blks; pos += 8) {
         active_cl[0] = ram_begin[pos + 0];
         pop_cnt += alex_PopCount(ram_begin[pos + 0]);

         active_cl[1] = ram_begin[pos + 1];
         pop_cnt += alex_PopCount(ram_begin[pos + 1]);

         active_cl[2] = ram_begin[pos + 2];
         pop_cnt += alex_PopCount(ram_begin[pos + 2]);

         active_cl[3] = ram_begin[pos + 3];
         pop_cnt += alex_PopCount(ram_begin[pos + 3]);

         active_cl[4] = ram_begin[pos + 4];
         pop_cnt += alex_PopCount(ram_begin[pos + 4]);

         active_cl[5] = ram_begin[pos + 5];
         pop_cnt += alex_PopCount(ram_begin[pos + 5]);

         active_cl[6] = ram_begin[pos + 6];
         pop_cnt += alex_PopCount(ram_begin[pos + 6]);

         active_cl[7] = ram_begin[pos + 7];
         pop_cnt += alex_PopCount(ram_begin[pos + 7]);

         alex_FlushClToNvm(nvm_begin, (uint8_t *) active_cl);
         memset((uint8_t *) active_cl, 0, 64);
         cl_pos = 0;
         nvm_begin += 64;
      }

      // Tail
      for (; pos<blks; pos++) {
         active_cl[cl_pos] = ram_begin[pos];
         pop_cnt += alex_PopCount(ram_begin[pos]);
         cl_pos++;
      }

      //      cout << "writing pop_cnt to " << (uint64_t(nvm_begin) - uint64_t(file.data)) + cl_pos << endl;
      active_cl[cl_pos] = pop_cnt;
      cl_pos++;
      alex_FlushClToNvm(nvm_begin, (uint8_t *) active_cl);
      alex_SFence();

      if (cl_pos % 8 == 0) {
         memset((uint8_t *) active_cl, 0, 64);
         cl_pos = 0;
         nvm_begin += 64;
      }

      // Advance and done
      next_free += entry.payload_size + 16;
      return next_free;
   }

   std::unique_ptr<Entry> GetNextLogEntry() // Read code is only to verify correctnes
   {
      if (log_read_offset == next_free) {
         return nullptr;
      }

      // Read length
      uint64_t len = *(uint64_t *) &file.data[log_read_offset];
      log_read_offset += 8;
      uint64_t pop_cnt = 0;
      pop_cnt += alex_PopCount(len);

      // Read data
      std::vector<uint64_t> result(len);
      for (uint32_t pos = 0; pos<len; pos += 8) {
         result[pos / 8] = *(uint64_t *) (file.data + log_read_offset);
         log_read_offset += 8;
         pop_cnt += alex_PopCount(result[pos / 8]);
      }

      // Read pop cnt
      uint64_t read_pop_cnt = *(uint64_t *) (file.data + log_read_offset);
      log_read_offset += 8;
      if (read_pop_cnt != pop_cnt) {
         std::cout << "read_pop_cnt does not match !! " << read_pop_cnt << " vs " << pop_cnt << std::endl;
         throw;
      }

      Entry *entry = new(malloc(sizeof(Entry) + result.size())) Entry();
      entry->payload_size = result.size();
      memcpy(entry->data, (uint8_t *) result.data(), result.size());
      return std::unique_ptr<Entry>(entry);
   }

   uint64_t GetWrittenByteCount() const
   {
      return next_free + sizeof(File);
   }
};
// -------------------------------------------------------------------------------------
template<uint32_t entry_size>
struct LogBasedUpdates {
   NonVolatileMemory nvm_log;
   NonVolatileMemory nvm_data;
   LogWriterZeroCached log_writer;
   std::unique_ptr<LogWriterZeroCached::Entry> entry;
   uint64_t entry_count;

   LogBasedUpdates(const std::string &path, uint64_t entry_count, uint64_t log_buffer_size)
           : nvm_log(path + "/log_log_file", log_buffer_size)
             , nvm_data(path + "/log_data_file", entry_count * entry_size)
             , log_writer(nvm_log)
             , entry(new(malloc(sizeof(LogWriterZeroCached::Entry) + entry_size)) LogWriterZeroCached::Entry())
             , entry_count(entry_count)
   {
      assert(nvm_log.GetByteCount()>=log_buffer_size);
      assert(nvm_data.GetByteCount()>=entry_size * entry_count);

      memset(nvm_data.Data(), 'a', nvm_data.GetByteCount());
      pmem_persist(nvm_data.Data(), nvm_data.GetByteCount());
   }

   void DoUpdate(uint64_t entry_id, LogWriterZeroCached::Entry *new_data)
   {
      assert(entry_id<entry_count);
      assert(new_data->payload_size == entry_size);

      new_data->entry_id = entry_id;
      log_writer.AddLogEntry(*new_data);

      ub1 *entry_begin = nvm_data.Data() + (entry_id * entry_size);
      alex_FastCopyAndWriteBack(entry_begin, new_data->data, entry_size);
      alex_SFence();
   }

   std::vector<LogWriterZeroCached::Entry *> PrepareUpdates(uint64_t count)
   {
      Random ranny;
      std::vector<LogWriterZeroCached::Entry *> results;
      for (uint64_t i = 0; i<count; i++) {
         LogWriterZeroCached::Entry *entry = (LogWriterZeroCached::Entry *) malloc(sizeof(LogWriterZeroCached::Entry) + entry_size);
         entry->payload_size = entry_size;
         char *str = CreateAlignedString(ranny, entry_size);
         memcpy(entry->data, str, entry_size);
         free(str);
         results.push_back(entry);
      }

      return results;
   }

   char *GetResult()
   {
      return (char *) nvm_data.Data();
   }
};
// -------------------------------------------------------------------------------------

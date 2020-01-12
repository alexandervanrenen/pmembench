#include "Common.hpp"
#include "NonVolatileMemory.hpp"
#include <memory>
// -------------------------------------------------------------------------------------
template<uint32_t entry_size>
struct LogWriterZeroCached {
   static_assert(entry_size % 8 == 0);

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

   uint64_t AddLogEntry(const UpdateOperation<entry_size> &entry)
   {
      uint32_t blks = entry_size / 8;

      assert(next_free % 8 == 0);
      assert(next_free + entry_size<nvm.GetByteCount());

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

         if (cl_pos % 8 == 0) {
            memset((uint8_t *) active_cl, 0, 64);
            cl_pos = 0;
            nvm_begin += 64;
         }
      }

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
      next_free += entry_size + 8;
      return next_free;
   }

   uint64_t GetWrittenByteCount() const
   {
      return next_free + sizeof(File);
   }
};
// -------------------------------------------------------------------------------------
template<uint32_t entry_size>
struct LogBasedUpdates {
   const static uint64_t LOG_BUFFER_SIZE = 30e9;
   NonVolatileMemory nvm_log;
   NonVolatileMemory nvm_data;
   LogWriterZeroCached<entry_size> log_writer;
   uint64_t entry_count;
   UpdateOperation<entry_size> *data_on_nvm;

   LogBasedUpdates(const std::string &path, uint64_t entry_count)
           : nvm_log(path + "/logbased_log_file", LOG_BUFFER_SIZE)
             , nvm_data(path + "/logbased_data_file", entry_count * sizeof(UpdateOperation<entry_size>))
             , log_writer(nvm_log)
             , entry_count(entry_count)
   {
      assert(nvm_log.GetByteCount()>=LOG_BUFFER_SIZE);
      assert(nvm_data.GetByteCount()>=entry_size * entry_count);

      memset(nvm_data.Data(), 'a', nvm_data.GetByteCount());
      pmem_persist(nvm_data.Data(), nvm_data.GetByteCount());

      data_on_nvm = (UpdateOperation<entry_size> *) nvm_data.Data();
   }

   ~LogBasedUpdates()
   {
      if (log_writer.GetWrittenByteCount()>=nvm_log.GetByteCount()) {
         std::cout << "write more log than we had space.. not good" << std::endl;
         exit(-1);
      }
   }

   void DoUpdate(const UpdateOperation<entry_size> &op)
   {
      assert(op.entry_id<entry_count);

      log_writer.AddLogEntry(op);

      ub1 *entry_begin = nvm_data.Data() + (op.entry_id * sizeof(UpdateOperation<entry_size>));
      alex_FastCopyAndWriteBack(entry_begin, (ub1 *) &op, entry_size);
      alex_SFence();
   }

   void ReadResult(std::vector<UpdateOperation<entry_size>> &result)
   {
      assert(result.size() == entry_count);
      for (uint64_t i = 0; i<entry_count; i++) {
         result[i] = data_on_nvm[i];
      }
   }

   void ReadSingleResult(UpdateOperation<entry_size> &result)
   {
      result = data_on_nvm[result.entry_id];
   }
};
// -------------------------------------------------------------------------------------

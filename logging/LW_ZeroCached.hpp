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

      assert(size >= 64);
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
         pop_cnt += alex_PopCount(ram_begin[pos]);
         cl_pos++;

         if (cl_pos % 8 == 0) {
            alex_FlushClToNvm(nvm_begin, (ub1 *) active_cl);
            memset((ub1 *) active_cl, 0, 64);
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

         alex_FlushClToNvm(nvm_begin, (ub1 *) active_cl);
         memset((ub1 *) active_cl, 0, 64);
         cl_pos = 0;
         nvm_begin += 64;
      }

      // Tail
      for (; pos<blks; pos++) {
         active_cl[cl_pos] = ram_begin[pos];
         pop_cnt += alex_PopCount(ram_begin[pos]);
         cl_pos++;
      }

      //      cout << "writing pop_cnt to " << (ub8(nvm_begin) - ub8(file.data)) + cl_pos << endl;
      active_cl[cl_pos] = pop_cnt;
      cl_pos++;
      alex_FlushClToNvm(nvm_begin, (ub1 *) active_cl);
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

   unique_ptr<Entry> GetNextLogEntry() // Read code is only to verify correctnes
   {
      if (log_read_offset == next_free) {
         return nullptr;
      }

      // Read length
      ub8 len = *(ub8 *) &file.data[log_read_offset];
      log_read_offset += 8;
      ub8 pop_cnt = 0;
      pop_cnt += alex_PopCount(len);

      // Read data
      vector<ub8> result(len);
      for (ub4 pos = 0; pos<len; pos += 8) {
         result[pos / 8] = *(ub8 *) (file.data + log_read_offset);
         log_read_offset += 8;
         pop_cnt += alex_PopCount(result[pos / 8]);
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

   static vector<Entry *> CreateRandomEntries(vector<ub1> &memory, ub4 min_size, ub4 max_size, ub8 log_payload_size, Random &ranny)
   {
      ub8 current = 0;
      ub8 used_size = 0;

      vector<Entry *> entries;
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
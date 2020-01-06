struct LogWriterMnemosyne {

   struct Entry {
      ub8 payload_size;
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
   ub8 log_read_offset;

   LogWriterMnemosyne(NonVolatileMemory &nvm)
           : nvm(nvm)
             , file(*reinterpret_cast<File *>(nvm.Data()))
   {
      next_free = 0;
      log_read_offset = 0;
   }

   ub8 AddLogEntry(const Entry &entry)
   {
      assert(next_free + entry.payload_size + 8<nvm.GetByteCount());
      assert(entry.payload_size>0 && entry.payload_size % 8 == 0);
      assert((uint64_t) &file.data[next_free] % 64 == 0);
      assert(next_free % 64 == 0);

      // Write length of log entry
      (uint64_t &) file.data[next_free] = entry.payload_size | 0x1;
      next_free += 8;

      // Write payload of log entry
      uint64_t buffer = 0;
      uint64_t checker = 0;

      uint32_t i = 0;
      for (i = 0; i<entry.payload_size; i += 8) {
         uint64_t cur = *(const uint64_t *) &entry.data[i];

         buffer = (buffer << 1) | (cur & 0x1);
         cur = (cur | 0x1);
         *((uint64_t *) &file.data[next_free]) = cur;
         next_free += 8;
         //         cout << i << ": " << buffer << endl;

         // Flush cache line every cache line
         if (next_free % 64 == 0) {
            alex_WriteBack(file.data + next_free - 8);
         }

         // Flush buffer every 63 8-byte blocks
         checker++;
         if (i>0 && (i + 8) % 504 == 0) {
            //            cout << "flush checker" << endl;
            assert(checker == 63);
            buffer = (buffer << 1) | 0x1;
            *((uint64_t *) &file.data[next_free]) = buffer;
            next_free += 8;
            checker = 0;
            buffer = 0;

            // Flush cache line every cache line
            if (next_free % 64 == 0) {
               alex_WriteBack(file.data + next_free - 8);
            }
         }
      }

      if (i % 504 != 0) {
         assert(checker>0);
         //         cout << "flush tail checker: " << buffer << " @" << next_free << endl;

         buffer = (buffer << 1) | 0x1;
         *((uint64_t *) &file.data[next_free]) = buffer;
         next_free += 8;
      }
      alex_WriteBack(file.data + next_free - 8);
      alex_SFence();

      // Advance and done
      next_free = (next_free + 63) & ~63ull;
      assert(next_free % 64 == 0);
      return next_free;
   }

   unique_ptr<Entry> GetNextLogEntry() // Read code is only to verify correctnes
   {
      if (log_read_offset == next_free) {
         return nullptr;
      }

      // Read length
      uint64_t len = *((uint64_t *) &file.data[log_read_offset]) & ~(0x1ull);
      log_read_offset += 8;

      //      cout << "got len: " << len << endl;

      uint64_t checker = 0;

      vector<char> result(len);
      uint32_t i = 0;
      for (; i<len; i += 8) {
         *((uint64_t *) &result[i]) = *((uint64_t *) &file.data[log_read_offset]);
         *((uint64_t *) &result[i]) &= ~(0x1ull);
         log_read_offset += 8;
         checker++;

         if (i>0 && (i + 8) % 504 == 0) {
            assert(checker == 63);
            checker = 0;
            //            cout << "apply checker" << endl;

            uint64_t buffer = *((uint64_t *) &file.data[log_read_offset]) >> 1;
            log_read_offset += 8;
            for (uint32_t c = 0; c<63; c++) {
               *((uint64_t *) &result[i - c * 8]) |= buffer & 0x1;
               buffer = buffer >> 1;
            }
         }
      }

      if (i % 504 != 0) {
         i -= 8;
         assert(checker>0);

         uint64_t buffer = *((uint64_t *) &file.data[log_read_offset]) >> 1;
         //         cout << "apply tail checker " << buffer << " @" << log_read_offset << endl;

         log_read_offset += 8;
         for (uint32_t c = 0; c<checker; c++) {
            *((uint64_t *) &result[i - c * 8]) |= buffer & 0x1;
            buffer = buffer >> 1;
         }
         checker = 0;
      }

      // Advance
      log_read_offset = (log_read_offset + 63) & ~63ull;
      assert(log_read_offset % 64 == 0);

      Entry *entry = new(malloc(sizeof(Entry) + result.size())) Entry();
      entry->payload_size = result.size();
      memcpy(entry->data, result.data(), result.size());
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
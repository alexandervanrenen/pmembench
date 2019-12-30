


struct LogWriterClassicCached {

   struct Entry {
      ub8 payload_size; // header
      ub1 data[];
      // footer: ub8 start;
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
   ub8 *cl_buffer;
   ub4 cl_offset = 0;

   LogWriterClassicCached(NonVolatileMemory &nvm)
           : nvm(nvm)
             , file(*reinterpret_cast<File *>(nvm.Data()))
   {
      next_free = 0;
      cl_buffer = reinterpret_cast<ub8 *>(operator new(128));
      while (((ub8) cl_buffer) % 64 != 0) {
         cl_buffer++;
      }
      memset(cl_buffer, 0, 64);
   }

   ub8 AddLogEntry(const Entry &entry)
   {
      assert(next_free % 8 == 0);
      assert(entry.payload_size % 8 == 0);
      assert(sizeof(File) + next_free + entry.payload_size + 16<nvm.GetByteCount());
      ub4 entry_size = entry.payload_size + 8;

      // Copy the log entry (header + body - footer)
      ub1 *nvm = (ub1 * )(((ub8)(file.data + next_free)) & ~ub8(63));
      assert(((ub8) nvm) % 64 == 0);
      const ub8 *entry_cur = reinterpret_cast<const ub8 *>(&entry);
      for (ub4 i = 0; i<entry_size; i += 8) {
         cl_buffer[cl_offset] = entry_cur[i];
         cl_offset++;
         if (cl_offset == 8) {
            cl_offset = 0;
            alex_FlushClToNvm(nvm, cl_buffer);
            nvm += 64;
         }
      }
      if (cl_offset>0) {
         alex_FlushClToNvm(nvm, cl_buffer);
      }
      alex_SFence();

      // Copy the footer
      cl_buffer[cl_offset] = next_free;
      cl_offset++;
      alex_FlushClToNvm(nvm, cl_buffer);
      if (cl_offset == 8) {
         cl_offset = 0;
         nvm += 64;
      }
      alex_SFence();

      // Advance and done
      next_free += entry.payload_size + 16;
      assert(next_free % 8 == 0);
      assert((ub8)nvm == (ub8(file.data + next_free) & ~ub8(63)));
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
         assert(current + payload_size + 16<memory.size());
         Entry *entry = (Entry *) &memory[current];
         entry->payload_size = payload_size;
         entries.push_back(entry);
         current += payload_size + 16;
         used_size += payload_size;
      }
      return entries;
   }
};
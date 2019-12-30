struct LogWriterZero {

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

   LogWriterZero(NonVolatileMemory &nvm)
           : nvm(nvm)
             , file(*reinterpret_cast<File *>(nvm.Data()))
   {
      next_free = 0;
   }

   ub8 AddLogEntry(const Entry &entry)
   {
      assert(next_free % 8 == 0);
      assert(entry.bit_count == 0);
      assert(entry.payload_size % 8 == 0);
      assert(sizeof(File) + next_free + entry.payload_size + 8<nvm.GetByteCount());

      // Copy the log entry (header + body - footer)
      ub4 pop_cnt = 0;
      const ub8 *entry_ram = reinterpret_cast<const ub8 *>(&entry);
      ub8 *entry_nvm = reinterpret_cast<ub8 *>(nvm.Data() + next_free);
      for (ub4 i = 0; i<entry.payload_size + 8; i += 8) {
         entry_nvm[i / 8] = entry_ram[i / 8];
         pop_cnt += alex_PopCount(entry_ram[i / 8]);
      }
      reinterpret_cast<Entry *>(entry_nvm)->bit_count = pop_cnt;
      alex_WriteBack(entry_nvm, entry.payload_size + 8);
      alex_SFence();

      // Advance and done
      next_free += entry.payload_size + 8;
      assert(next_free % 8 == 0);
      return next_free;
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
         entry->bit_count = 0;
         entries.push_back(entry);
         current += payload_size + 8;
         used_size += payload_size;
      }
      return entries;
   }
};
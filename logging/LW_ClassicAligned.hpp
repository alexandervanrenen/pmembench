struct LogWriterClassicAligned {

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

   LogWriterClassicAligned(NonVolatileMemory &nvm)
           : nvm(nvm)
             , file(*reinterpret_cast<File *>(nvm.Data()))
   {
      next_free = 0;
   }

   ub8 AddLogEntry(const Entry &entry)
   {
      assert(next_free % 8 == 0);
      assert(entry.payload_size % 8 == 0);
      assert(sizeof(File) + next_free + entry.payload_size + 16<nvm.GetByteCount());

      // Copy the log entry (header + body - footer)
      ub1 *entry_begin = file.data + next_free;
      const ub1 *ram_bgein = reinterpret_cast<const ub1 *>(&entry);
      alex_FastCopyAndWriteBack(entry_begin, ram_bgein, entry.payload_size + 8);
      alex_SFence();
      next_free = (next_free + entry.payload_size + 8 + 63) & ~63ull;

      // Copy the footer
      ub1 *footer_begin = file.data + next_free;
      *reinterpret_cast<ub8 *>(footer_begin) = next_free;
      alex_WriteBack(footer_begin);
      alex_SFence();

      // Advance and done
      next_free += 64;
      assert(next_free % 8 == 0);
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
struct LogWriterHeaderAligned {

   struct Entry {
      ub8 payload_size;
      ub1 data[];
   };

   struct File {
      // Header
      ub8 next_free;
      ub1 padding[constants::kCacheLineByteCount - sizeof(ub8)];

      // Log data
      ub1 data[];
   };
   static_assert(sizeof(File) == 64, "");
   ub8 next_free;

   NonVolatileMemory &nvm;
   File &file; // == nvm

   LogWriterHeaderAligned(NonVolatileMemory &nvm)
           : nvm(nvm)
             , file(*reinterpret_cast<File *>(nvm.Data()))
   {
      file.next_free = 0;
      next_free = 0;
   }

   ub8 AddLogEntry(const Entry &entry)
   {
      assert(entry.payload_size % 8 == 0);
      assert(next_free % 64 == 0);
      assert(sizeof(File) + next_free + entry.payload_size + 8<nvm.GetByteCount());
      ub8 size = entry.payload_size + 8;

      // Copy the log entry (header + body)
      ub1 *entry_begin = file.data + next_free;
      const ub1 *ram_begin = reinterpret_cast<const ub1 *>(&entry);
      alex_FastCopyAndWriteBack(entry_begin, ram_begin, size);
      alex_SFence();

      // Update the header
      next_free += size;
      next_free = (next_free + 63) & ~63ull; // 64 byte aligned
      file.next_free = next_free;
      alex_WriteBack((void *) &file);
      alex_SFence();

      assert(next_free % 64 == 0);
      return next_free;
   }

   ub8 GetWrittenByteCount() const
   {
      return file.next_free + sizeof(File);
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
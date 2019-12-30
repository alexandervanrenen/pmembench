struct LogWriterZeroSimd {

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

   LogWriterZeroSimd(NonVolatileMemory &nvm)
           : nvm(nvm)
             , file(*reinterpret_cast<File *>(nvm.Data()))
   {
      next_free = 0;
   }

   void FastCopy512Simd(ub1 *dest, const ub1 *src)
   {
      assert(((ub8) dest) % 64 == 0);
#ifdef __APPLE__
      memcpy(dest, src, 64);
#else
      //      __m256 reg0 = _mm256_loadu_ps((float*)(src + 0));
      //      _mm256_store_ps((float*)(dest + 0), reg0);
      //      __m256 reg1 = _mm256_loadu_ps((float*)(src + 32));
      //      _mm256_store_ps((float*)(dest + 32), reg1);
            __m512i reg = _mm512_loadu_si512(src);
            _mm512_store_si512((__m512i *) dest, reg);
#endif
   }

   ub4 FastPopCount512Simd(const ub1 *ptr)
   {
      ub4 res = 0;
      for (ub4 i = 0; i<64; i += 8) {
         res += alex_PopCount(*(ub8 *) (&ptr[i]));
      }
      return res;
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
      FastCopy512Simd(nvm_begin, ram_begin);
      pop_cnt += FastPopCount512Simd(ram_begin);

      // Copy remaining full cache lines (and flush)
      ub4 pos = 64;
      for (; pos + 63<size; pos += 64) {
         FastCopy512(nvm_begin + pos, ram_begin + pos);
         pop_cnt += FastPopCount512(ram_begin + pos);
         alex_WriteBack(nvm_begin + pos);
      }

      // Copy remaining bytes (and flush)
      if (pos<size) {
         for (; pos<size; pos += 8) {
            *(ub8 *) (nvm_begin + pos) = *(ub8 *) (ram_begin + pos);
            pop_cnt += alex_PopCount(*(ub8 *) (ram_begin + pos));
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
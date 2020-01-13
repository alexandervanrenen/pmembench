#include "Common.hpp"
#include "NonVolatileMemory.hpp"
#include <memory>
// -------------------------------------------------------------------------------------
namespace cow {
// -------------------------------------------------------------------------------------
template<uint32_t BYTE_COUNT>
struct InplaceCow {
   alignas(64) bool is_a_active; // Start at a cl to allow the use of streaming ops
   alignas(64) std::array<char, BYTE_COUNT> a;
   std::array<char, BYTE_COUNT> b; // Start at a cl to allow the use of streaming ops

   void Write(const char *input)
   {
      bool is_a_active_cache = is_a_active;
      if (is_a_active_cache) {
         alex_FastCopyAndWriteBack((ub1 *) b.data(), (const ub1 *) input, BYTE_COUNT);
         alex_SFence();
      } else {
         alex_FastCopyAndWriteBack((ub1 *) a.data(), (const ub1 *) input, BYTE_COUNT);
         alex_SFence();
      }

      is_a_active = !is_a_active_cache;
      alex_WriteBack(&is_a_active);
      alex_SFence();
   }

   void Read(char *output)
   {
      if (is_a_active) {
         memcpy(output, a.data(), BYTE_COUNT);
      } else {
         memcpy(output, b.data(), BYTE_COUNT);
      }
   }

   void Init(Random &ranny)
   {
      is_a_active = true; // ranny.Rand() % 2; // Make it so that the branch is ~50%
   }
};
// -------------------------------------------------------------------------------------
template<>
struct InplaceCow<16> {
   alignas(64) uint8_t active_version_id; // Start at a cl to allow the use of streaming ops
   std::array<std::array<char, 16>, 2> versions;

   void Write(const char *input)
   {
      assert((uint64_t) this % 64 == 0);
      assert((uint64_t) input % 64 == 0);
      assert((void *) &active_version_id == (void *) this);
      assert(active_version_id == 0 || active_version_id == 1);

      // Load cl and update
      if (active_version_id == 0) {
         memcpy(versions[1].data(), input, 16);
      } else {
         memcpy(versions[0].data(), input, 16);
      }

      // Write new data
      __m512i reg = _mm512_loadu_si512(this);
      _mm512_stream_si512((__m512i *) this, reg);
      alex_SFence();

      // Update version id
      __m512i mask = _mm512_castsi128_si512(_mm_cvtsi32_si128(0x01));
      reg = _mm512_xor_si512(reg, mask);

      // Write new version id
      _mm512_stream_si512((__m512i *) this, reg);
      alex_SFence();
   }

   void Read(char *output)
   {
      memcpy(output, versions[active_version_id].data(), 16);
   }

   void Init(Random &ranny)
   {
      active_version_id = ranny.Rand() % 2; // Make it so that the branch is ~50%
   }
};
// -------------------------------------------------------------------------------------
// 32 case is always 2 cls .. no chance to speed it up by packing the one version into the first cl
// -------------------------------------------------------------------------------------
template<>
struct InplaceCow<48> {
   alignas(64) uint8_t active_version_id; // Start at a cl to allow the use of streaming ops
   std::array<char, 48> a;
   alignas(64) std::array<char, 48> b;

   void Write(const char *input)
   {
      assert((uint64_t) this % 64 == 0);
      assert((uint64_t) input % 64 == 0);
      assert((void *) &active_version_id == (void *) this);
      assert(active_version_id == 0 || active_version_id == 1);

      // Load cl and update
      if (active_version_id == 0) {
         // Version and data on different cls -> use clwb, cause no need for streaming
         alex_FastCopyAndWriteBack((ub1 *) b.data(), (const ub1 *) input, 48);
         alex_SFence();

         active_version_id = active_version_id ^ 0x1;
         alex_WriteBack(&active_version_id);
         alex_SFence();
      } else {
         // Version and data both on first cl -> use streaming, because this cache line is re-written
         memcpy(a.data(), input, 48);

         // Write new data
         __m512i reg = _mm512_loadu_si512(this);
         _mm512_stream_si512((__m512i *) this, reg);
         alex_SFence();

         // Update version id
         __m512i mask = _mm512_castsi128_si512(_mm_cvtsi32_si128(0x01));
         reg = _mm512_xor_si512(reg, mask);

         // Write new version id
         _mm512_stream_si512((__m512i *) this, reg);
         alex_SFence();
      }
   }

   void Read(char *output)
   {
      if (active_version_id == 0) {
         memcpy(output, a.data(), 48);
      } else {
         memcpy(output, b.data(), 48);
      }
   }

   void Init(Random &ranny)
   {
      active_version_id = ranny.Rand() % 2; // Make it so that the branch is ~50%
   }
};
// -------------------------------------------------------------------------------------
// For larger sizes it matters less and less ..
// -------------------------------------------------------------------------------------
template<uint32_t entry_size>
struct CowBasedUpdates {
   NonVolatileMemory nvm_data;
   uint64_t entry_count;
   InplaceCow<entry_size> *entries;

   CowBasedUpdates(const std::string &path, uint64_t entry_count)
           : nvm_data(path + "/cowbased_data_file", entry_count * sizeof(InplaceCow<entry_size>))
             , entry_count(entry_count)
   {
      assert(nvm_data.GetByteCount()>=entry_size * entry_count);

      memset(nvm_data.Data(), 'a', nvm_data.GetByteCount());
      entries = (InplaceCow<entry_size> *) nvm_data.Data();
      Random ranny;
      for (uint32_t i = 0; i<entry_count; i++) {
         entries[i].Init(ranny);
      }
      pmem_persist(nvm_data.Data(), nvm_data.GetByteCount());
   }

   void DoUpdate(const Operation<entry_size> &op)
   {
      assert(op.entry_id<entry_count);
      entries[op.entry_id].Write((const char *) &op);
   }

   uint64_t ReadSingleResult(Operation<entry_size> &result)
   {
      entries[result.entry_id].Read((char *) &result);
      return result.entry_id;
   }
};
// -------------------------------------------------------------------------------------
}
// -------------------------------------------------------------------------------------
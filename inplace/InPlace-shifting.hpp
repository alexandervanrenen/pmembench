#pragma once
// -------------------------------------------------------------------------------------
#include "Common.hpp"
#include "NonVolatileMemory.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <array>
// -------------------------------------------------------------------------------------
namespace v3 {
// -------------------------------------------------------------------------------------
template<uint32_t pos>
struct Block {
   static_assert(pos<=31);

   uint64_t data;

   Block()
           : data(0) {}

   uint32_t GetVersionNoCheck() const { return (((data & (1ull << (pos + 32))) << 1) | (data & (1ull << pos))) >> pos; }
   uint32_t GetOldStateNoCheck() const { return (data >> 32) & ~(1ull << pos); }
   uint32_t GetNewStateNoCheck() const { return (data & 0xffffffff) & ~(1ull << pos); }

   inline void WriteNoCheck(uint32_t new_state)
   {
      data = (data << 32) | new_state;
   }

   friend std::ostream &operator<<(std::ostream &os, const Block &b)
   {
      uint32_t version = b.GetVersionNoCheck();
      uint32_t old_state = b.GetOldStateNoCheck();
      uint32_t new_state = b.GetNewStateNoCheck();
      os << "version: " << version << " old: ";
      DumpHex(&old_state, 4, os);
      os << " new: ";
      DumpHex(&new_state, 4, os);
      return os;
   }
};
static_assert(sizeof(Block<0>) == 8);
// -------------------------------------------------------------------------------------
template<uint32_t BYTE_COUNT>
struct InplaceField {
   static const uint32_t BIT_COUNT = BYTE_COUNT * 8;
   static const uint32_t BLOCK_COUNT = (BIT_COUNT + 30) / 31;

   alignas(64)
   uint64_t blocks[BLOCK_COUNT];

   void Reset()
   {
      memset(blocks, 0, sizeof(uint64_t) * BLOCK_COUNT);
   }

   static constexpr std::array<uint32_t, 4> version_bit = {1, 1, 0, 0};
   static constexpr std::array<uint32_t, 2> masks = {0, ~uint32_t(0)};

   // count is in 4 byte
   template<int32_t count>
   inline void WriteRec(const char *data)
   {
      uint32_t *input = (uint32_t *) data;
      uint32_t current_version_bit = (blocks[0] & 0x10) >> 4; // BUG !! need bit from old version
      uint32_t next_version_bit = version_bit[current_version_bit];
      uint32_t mask = masks[next_version_bit];

      uint32_t high_bits = 0;

      static_assert(count == 4);

      //@formatter:off
      if constexpr(count>=1) { high_bits |= input[0] & 0x1; blocks[0] = (blocks[0] << 32) | ((input[0] & ~0x1) | (0x1 & mask)); }
      if constexpr(count>=2) { high_bits |= input[1] & 0x2; blocks[1] = (blocks[1] << 32) | ((input[1] & ~0x2) | (0x2 & mask)); }
      if constexpr(count>=3) { high_bits |= input[2] & 0x4; blocks[2] = (blocks[2] << 32) | ((input[2] & ~0x4) | (0x4 & mask)); }
      if constexpr(count>=4) { high_bits |= input[3] & 0x8; blocks[3] = (blocks[3] << 32) | ((input[3] & ~0x8) | (0x8 & mask)); }
      //@formatter:on

      blocks[4] = (blocks[4] << 32) | high_bits | (next_version_bit << 4);

      WriteRec<count - 31>(data + 31);
   }

   //@formatter:off
   template<> inline void WriteRec<0>(const char *data) {}
   template<> inline void WriteRec<-1>(const char *data) {}
   template<> inline void WriteRec<-2>(const char *data) {}
   template<> inline void WriteRec<-3>(const char *data) {}
   template<> inline void WriteRec<-4>(const char *data) {}
   template<> inline void WriteRec<-5>(const char *data) {}
   template<> inline void WriteRec<-6>(const char *data) {}
   template<> inline void WriteRec<-7>(const char *data) {}
   template<> inline void WriteRec<-8>(const char *data) {}
   template<> inline void WriteRec<-9>(const char *data) {}
   template<> inline void WriteRec<-10>(const char *data) {}
   template<> inline void WriteRec<-11>(const char *data) {}
   template<> inline void WriteRec<-12>(const char *data) {}
   template<> inline void WriteRec<-13>(const char *data) {}
   template<> inline void WriteRec<-14>(const char *data) {}
   template<> inline void WriteRec<-15>(const char *data) {}
   template<> inline void WriteRec<-16>(const char *data) {}
   template<> inline void WriteRec<-17>(const char *data) {}
   template<> inline void WriteRec<-18>(const char *data) {}
   template<> inline void WriteRec<-19>(const char *data) {}
   template<> inline void WriteRec<-20>(const char *data) {}
   template<> inline void WriteRec<-21>(const char *data) {}
   template<> inline void WriteRec<-22>(const char *data) {}
   template<> inline void WriteRec<-23>(const char *data) {}
   template<> inline void WriteRec<-24>(const char *data) {}
   template<> inline void WriteRec<-25>(const char *data) {}
   template<> inline void WriteRec<-26>(const char *data) {}
   template<> inline void WriteRec<-27>(const char *data) {}
   template<> inline void WriteRec<-28>(const char *data) {}
   template<> inline void WriteRec<-29>(const char *data) {}
   template<> inline void WriteRec<-30>(const char *data) {}
   //@formatter:on

   void WriteNoCheck(const char *data)
   {
      assert((uint64_t) data % 4 == 0);
      assert((uint64_t) &blocks[0] % 64 == 0);

      WriteRec<4>(data);
   }

   static void Dump256(__m256i val)
   {
      char arr[32];
      _mm256_storeu_si256((__m256i_u *) arr, val);
      DumpHex(arr, 32, std::cout);
      std::cout << std::endl;
   }

   static void Dump128(__m128i val)
   {
      char arr[16];
      _mm_storeu_si128((__m128i_u *) arr, val);
      DumpHex(arr, 16, std::cout);
      std::cout << std::endl;
   }

   void ReadNoCheck(char *result)
   {
      uint32_t *output = (uint32_t *) result;

      output[0] = (blocks[0] & 0xfffffffe) | (blocks[4] & 0x1);
      output[1] = (blocks[1] & 0xfffffffd) | (blocks[4] & 0x2);
      output[2] = (blocks[2] & 0xfffffffb) | (blocks[4] & 0x4);
      output[3] = (blocks[3] & 0xfffffff7) | (blocks[4] & 0x8);
   }
};
// -------------------------------------------------------------------------------------
template<uint32_t entry_size>
struct InPlaceLikeUpdates {

   static bool CanBeUsed(uint32_t entry_size_param) { return entry_size_param == 16; }

   NonVolatileMemory nvm_data;
   uint64_t entry_count;
   InplaceField<entry_size> *entries;

   InPlaceLikeUpdates(const std::string &path, uint64_t entry_count)
           : nvm_data(path + "/inplace2_file", sizeof(InplaceField<entry_size>) * entry_count)
             , entry_count(entry_count)
   {
      std::vector<char> data(entry_size, 'a');
      entries = (InplaceField<entry_size> *) nvm_data.Data();
      for (uint64_t i = 0; i<entry_count; i++) {
         entries[i].Reset();
         entries[i].WriteNoCheck(data.data());
      }
   }

   void DoUpdate(const UpdateOperation<entry_size> &op)
   {
      entries[op.entry_id].WriteNoCheck((const char *) &op);
      for (uint32_t i = 0; i<sizeof(InplaceField<entry_size>); i += 64) {
         char *addr = (char *) (entries + op.entry_id) + i;
         assert((uint64_t) addr % 64 == 0);
         alex_WriteBack(addr);
      }
      alex_SFence();
   }

   void ReadResult(std::vector<UpdateOperation<entry_size>> &result)
   {
      assert(result.size() == entry_count);
      for (uint64_t i = 0; i<entry_count; i++) {
         entries[i].ReadNoCheck((char *) &result[i]);
      }
   }

   void ReadSingleResult(UpdateOperation<entry_size> &result)
   {
      entries[result.entry_id].ReadNoCheck((char *) &result);
   }
};
// -------------------------------------------------------------------------------------
}
// -------------------------------------------------------------------------------------

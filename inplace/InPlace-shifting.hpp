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

      //@formatter:off
      if constexpr(count>=1)  { high_bits |= input[ 0] & 0x0001; blocks[ 0] = (blocks[ 0] << 32) | ((input[ 0] & ~0x0001) | (0x0001 & mask)); }
      if constexpr(count>=2)  { high_bits |= input[ 1] & 0x0002; blocks[ 1] = (blocks[ 1] << 32) | ((input[ 1] & ~0x0002) | (0x0002 & mask)); }
      if constexpr(count>=3)  { high_bits |= input[ 2] & 0x0004; blocks[ 2] = (blocks[ 2] << 32) | ((input[ 2] & ~0x0004) | (0x0004 & mask)); }
      if constexpr(count>=4)  { high_bits |= input[ 3] & 0x0008; blocks[ 3] = (blocks[ 3] << 32) | ((input[ 3] & ~0x0008) | (0x0008 & mask)); }
      if constexpr(count>=5)  { high_bits |= input[ 4] & 0x0010; blocks[ 4] = (blocks[ 4] << 32) | ((input[ 4] & ~0x0010) | (0x0010 & mask)); }
      if constexpr(count>=6)  { high_bits |= input[ 5] & 0x0020; blocks[ 5] = (blocks[ 5] << 32) | ((input[ 5] & ~0x0020) | (0x0020 & mask)); }
      if constexpr(count>=7)  { high_bits |= input[ 6] & 0x0040; blocks[ 6] = (blocks[ 6] << 32) | ((input[ 6] & ~0x0040) | (0x0040 & mask)); }
      if constexpr(count>=8)  { high_bits |= input[ 7] & 0x0080; blocks[ 7] = (blocks[ 7] << 32) | ((input[ 7] & ~0x0080) | (0x0080 & mask)); }
      if constexpr(count>=9)  { high_bits |= input[ 8] & 0x0100; blocks[ 8] = (blocks[ 8] << 32) | ((input[ 8] & ~0x0100) | (0x0100 & mask)); }
      if constexpr(count>=10) { high_bits |= input[ 9] & 0x0200; blocks[ 9] = (blocks[ 9] << 32) | ((input[ 9] & ~0x0200) | (0x0200 & mask)); }
      if constexpr(count>=11) { high_bits |= input[10] & 0x0400; blocks[10] = (blocks[10] << 32) | ((input[10] & ~0x0400) | (0x0400 & mask)); }
      if constexpr(count>=12) { high_bits |= input[11] & 0x0800; blocks[11] = (blocks[11] << 32) | ((input[11] & ~0x0800) | (0x0800 & mask)); }
      if constexpr(count>=13) { high_bits |= input[12] & 0x1000; blocks[12] = (blocks[12] << 32) | ((input[12] & ~0x1000) | (0x1000 & mask)); }
      if constexpr(count>=14) { high_bits |= input[13] & 0x2000; blocks[13] = (blocks[13] << 32) | ((input[13] & ~0x2000) | (0x2000 & mask)); }
      if constexpr(count>=15) { high_bits |= input[14] & 0x4000; blocks[14] = (blocks[14] << 32) | ((input[14] & ~0x4000) | (0x4000 & mask)); }
      if constexpr(count>=16) { high_bits |= input[15] & 0x8000; blocks[15] = (blocks[15] << 32) | ((input[15] & ~0x8000) | (0x8000 & mask)); }
      //@formatter:on

      blocks[count] = (blocks[count] << 32) | high_bits | (next_version_bit << count);

      //      WriteRec<count - 31>(data + 31);
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

      WriteRec<BYTE_COUNT / 4>(data);
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

   // count is in 4 byte
   template<int32_t count>
   inline void ReadRec(char *result)
   {
      uint32_t *output = (uint32_t *) result;

      //@formatter:off
      if constexpr(count>=1)  { output[ 0] = (blocks[ 0] & 0xfffffffe) | (blocks[count] & 0x0001); }
      if constexpr(count>=2)  { output[ 1] = (blocks[ 1] & 0xfffffffd) | (blocks[count] & 0x0002); }
      if constexpr(count>=3)  { output[ 2] = (blocks[ 2] & 0xfffffffb) | (blocks[count] & 0x0004); }
      if constexpr(count>=4)  { output[ 3] = (blocks[ 3] & 0xfffffff7) | (blocks[count] & 0x0008); }
      if constexpr(count>=5)  { output[ 4] = (blocks[ 4] & 0xffffffef) | (blocks[count] & 0x0010); }
      if constexpr(count>=6)  { output[ 5] = (blocks[ 5] & 0xffffffdf) | (blocks[count] & 0x0020); }
      if constexpr(count>=7)  { output[ 6] = (blocks[ 6] & 0xffffffbf) | (blocks[count] & 0x0040); }
      if constexpr(count>=8)  { output[ 7] = (blocks[ 7] & 0xffffff7f) | (blocks[count] & 0x0080); }
      if constexpr(count>=9)  { output[ 8] = (blocks[ 8] & 0xfffffeff) | (blocks[count] & 0x0100); }
      if constexpr(count>=10) { output[ 9] = (blocks[ 9] & 0xfffffdff) | (blocks[count] & 0x0200); }
      if constexpr(count>=11) { output[10] = (blocks[10] & 0xfffffbff) | (blocks[count] & 0x0400); }
      if constexpr(count>=12) { output[11] = (blocks[11] & 0xfffff7ff) | (blocks[count] & 0x0800); }
      if constexpr(count>=13) { output[12] = (blocks[12] & 0xffffefff) | (blocks[count] & 0x1000); }
      if constexpr(count>=14) { output[13] = (blocks[13] & 0xffffdfff) | (blocks[count] & 0x2000); }
      if constexpr(count>=15) { output[14] = (blocks[14] & 0xffffbfff) | (blocks[count] & 0x4000); }
      if constexpr(count>=16) { output[15] = (blocks[15] & 0xffff7fff) | (blocks[count] & 0x8000); }
      //@formatter:on

      //      ReadRec<count - 31>(result + 31);
   }

   //@formatter:off
   template<> inline void ReadRec<0>(char *data) {}
   template<> inline void ReadRec<-1>(char *data) {}
   template<> inline void ReadRec<-2>(char *data) {}
   template<> inline void ReadRec<-3>(char *data) {}
   template<> inline void ReadRec<-4>(char *data) {}
   template<> inline void ReadRec<-5>(char *data) {}
   template<> inline void ReadRec<-6>(char *data) {}
   template<> inline void ReadRec<-7>(char *data) {}
   template<> inline void ReadRec<-8>(char *data) {}
   template<> inline void ReadRec<-9>(char *data) {}
   template<> inline void ReadRec<-10>(char *data) {}
   template<> inline void ReadRec<-11>(char *data) {}
   template<> inline void ReadRec<-12>(char *data) {}
   template<> inline void ReadRec<-13>(char *data) {}
   template<> inline void ReadRec<-14>(char *data) {}
   template<> inline void ReadRec<-15>(char *data) {}
   template<> inline void ReadRec<-16>(char *data) {}
   template<> inline void ReadRec<-17>(char *data) {}
   template<> inline void ReadRec<-18>(char *data) {}
   template<> inline void ReadRec<-19>(char *data) {}
   template<> inline void ReadRec<-20>(char *data) {}
   template<> inline void ReadRec<-21>(char *data) {}
   template<> inline void ReadRec<-22>(char *data) {}
   template<> inline void ReadRec<-23>(char *data) {}
   template<> inline void ReadRec<-24>(char *data) {}
   template<> inline void ReadRec<-25>(char *data) {}
   template<> inline void ReadRec<-26>(char *data) {}
   template<> inline void ReadRec<-27>(char *data) {}
   template<> inline void ReadRec<-28>(char *data) {}
   template<> inline void ReadRec<-29>(char *data) {}
   template<> inline void ReadRec<-30>(char *data) {}
   //@formatter:on

   void ReadNoCheck(char *result)
   {
      ReadRec<BYTE_COUNT / 4>(result);
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

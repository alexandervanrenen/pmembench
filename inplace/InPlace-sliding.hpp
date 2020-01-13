#pragma once
// -------------------------------------------------------------------------------------
#include "Common.hpp"
#include "NonVolatileMemory.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <array>
// -------------------------------------------------------------------------------------
namespace sliding {
// -------------------------------------------------------------------------------------
template<uint32_t BYTE_COUNT>
struct InplaceField {
   static const uint32_t BIT_COUNT = BYTE_COUNT * 8;
   static const uint32_t BLOCK_COUNT = (BIT_COUNT + 30) / 31;

   alignas(64)
   uint64_t _blocks[BLOCK_COUNT];

   void Reset()
   {
      memset(_blocks, 0, sizeof(uint64_t) * BLOCK_COUNT);
   }

   static constexpr std::array<uint32_t, 2> version_bit = {1, 0};
   static constexpr std::array<uint32_t, 2> masks = {0, ~uint32_t(0)};

   // count is in 4 byte
   template<int32_t count>
   inline void WriteRec(const uint32_t *input, uint64_t *blocks)
   {
      uint32_t current_version_bit = (blocks[0] >> 32) & 0x1;
      uint32_t next_version_bit = version_bit[current_version_bit];
      uint32_t mask = masks[next_version_bit];

      uint32_t high_bits = 0;

      //@formatter:off
      if constexpr(count>=1)  { high_bits |= input[ 0] & 0x00000001; blocks[ 0] = (blocks[ 0] << 32) | ((input[ 0] & ~0x00000001) | (0x00000001 & mask)); }
      if constexpr(count>=2)  { high_bits |= input[ 1] & 0x00000002; blocks[ 1] = (blocks[ 1] << 32) | ((input[ 1] & ~0x00000002) | (0x00000002 & mask)); }
      if constexpr(count>=3)  { high_bits |= input[ 2] & 0x00000004; blocks[ 2] = (blocks[ 2] << 32) | ((input[ 2] & ~0x00000004) | (0x00000004 & mask)); }
      if constexpr(count>=4)  { high_bits |= input[ 3] & 0x00000008; blocks[ 3] = (blocks[ 3] << 32) | ((input[ 3] & ~0x00000008) | (0x00000008 & mask)); }
      if constexpr(count>=5)  { high_bits |= input[ 4] & 0x00000010; blocks[ 4] = (blocks[ 4] << 32) | ((input[ 4] & ~0x00000010) | (0x00000010 & mask)); }
      if constexpr(count>=6)  { high_bits |= input[ 5] & 0x00000020; blocks[ 5] = (blocks[ 5] << 32) | ((input[ 5] & ~0x00000020) | (0x00000020 & mask)); }
      if constexpr(count>=7)  { high_bits |= input[ 6] & 0x00000040; blocks[ 6] = (blocks[ 6] << 32) | ((input[ 6] & ~0x00000040) | (0x00000040 & mask)); }
      if constexpr(count>=8)  { high_bits |= input[ 7] & 0x00000080; blocks[ 7] = (blocks[ 7] << 32) | ((input[ 7] & ~0x00000080) | (0x00000080 & mask)); }
      if constexpr(count>=9)  { high_bits |= input[ 8] & 0x00000100; blocks[ 8] = (blocks[ 8] << 32) | ((input[ 8] & ~0x00000100) | (0x00000100 & mask)); }
      if constexpr(count>=10) { high_bits |= input[ 9] & 0x00000200; blocks[ 9] = (blocks[ 9] << 32) | ((input[ 9] & ~0x00000200) | (0x00000200 & mask)); }
      if constexpr(count>=11) { high_bits |= input[10] & 0x00000400; blocks[10] = (blocks[10] << 32) | ((input[10] & ~0x00000400) | (0x00000400 & mask)); }
      if constexpr(count>=12) { high_bits |= input[11] & 0x00000800; blocks[11] = (blocks[11] << 32) | ((input[11] & ~0x00000800) | (0x00000800 & mask)); }
      if constexpr(count>=13) { high_bits |= input[12] & 0x00001000; blocks[12] = (blocks[12] << 32) | ((input[12] & ~0x00001000) | (0x00001000 & mask)); }
      if constexpr(count>=14) { high_bits |= input[13] & 0x00002000; blocks[13] = (blocks[13] << 32) | ((input[13] & ~0x00002000) | (0x00002000 & mask)); }
      if constexpr(count>=15) { high_bits |= input[14] & 0x00004000; blocks[14] = (blocks[14] << 32) | ((input[14] & ~0x00004000) | (0x00004000 & mask)); }
      if constexpr(count>=16) { high_bits |= input[15] & 0x00008000; blocks[15] = (blocks[15] << 32) | ((input[15] & ~0x00008000) | (0x00008000 & mask)); }
      if constexpr(count>=17) { high_bits |= input[16] & 0x00010000; blocks[16] = (blocks[16] << 32) | ((input[16] & ~0x00010000) | (0x00010000 & mask)); }
      if constexpr(count>=18) { high_bits |= input[17] & 0x00020000; blocks[17] = (blocks[17] << 32) | ((input[17] & ~0x00020000) | (0x00020000 & mask)); }
      if constexpr(count>=19) { high_bits |= input[18] & 0x00040000; blocks[18] = (blocks[18] << 32) | ((input[18] & ~0x00040000) | (0x00040000 & mask)); }
      if constexpr(count>=20) { high_bits |= input[19] & 0x00080000; blocks[19] = (blocks[19] << 32) | ((input[19] & ~0x00080000) | (0x00080000 & mask)); }
      if constexpr(count>=21) { high_bits |= input[20] & 0x00100000; blocks[20] = (blocks[20] << 32) | ((input[20] & ~0x00100000) | (0x00100000 & mask)); }
      if constexpr(count>=22) { high_bits |= input[21] & 0x00200000; blocks[21] = (blocks[21] << 32) | ((input[21] & ~0x00200000) | (0x00200000 & mask)); }
      if constexpr(count>=23) { high_bits |= input[22] & 0x00400000; blocks[22] = (blocks[22] << 32) | ((input[22] & ~0x00400000) | (0x00400000 & mask)); }
      if constexpr(count>=24) { high_bits |= input[23] & 0x00800000; blocks[23] = (blocks[23] << 32) | ((input[23] & ~0x00800000) | (0x00800000 & mask)); }
      if constexpr(count>=25) { high_bits |= input[24] & 0x01000000; blocks[24] = (blocks[24] << 32) | ((input[24] & ~0x01000000) | (0x01000000 & mask)); }
      if constexpr(count>=26) { high_bits |= input[25] & 0x02000000; blocks[25] = (blocks[25] << 32) | ((input[25] & ~0x02000000) | (0x02000000 & mask)); }
      if constexpr(count>=27) { high_bits |= input[26] & 0x04000000; blocks[26] = (blocks[26] << 32) | ((input[26] & ~0x04000000) | (0x04000000 & mask)); }
      if constexpr(count>=28) { high_bits |= input[27] & 0x08000000; blocks[27] = (blocks[27] << 32) | ((input[27] & ~0x08000000) | (0x08000000 & mask)); }
      if constexpr(count>=29) { high_bits |= input[28] & 0x10000000; blocks[28] = (blocks[28] << 32) | ((input[28] & ~0x10000000) | (0x10000000 & mask)); }
      if constexpr(count>=30) { high_bits |= input[29] & 0x20000000; blocks[29] = (blocks[29] << 32) | ((input[29] & ~0x20000000) | (0x20000000 & mask)); }
      if constexpr(count>=31) { high_bits |= input[30] & 0x40000000; blocks[30] = (blocks[30] << 32) | ((input[30] & ~0x40000000) | (0x40000000 & mask)); }
      //@formatter:on

      constexpr uint32_t meta_block_idx = count>=32 ? 31 : count;
      blocks[meta_block_idx] = (blocks[count] << 32) | high_bits | (next_version_bit << 31);

      WriteRec<count - 31>(input + 31, blocks + 32);
   }

   //@formatter:off
   template<> inline void WriteRec<0>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-1>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-2>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-3>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-4>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-5>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-6>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-7>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-8>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-9>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-10>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-11>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-12>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-13>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-14>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-15>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-16>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-17>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-18>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-19>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-20>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-21>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-22>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-23>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-24>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-25>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-26>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-27>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-28>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-29>(const uint32_t *input, uint64_t* blocks) {}
   template<> inline void WriteRec<-30>(const uint32_t *input, uint64_t* blocks) {}
   //@formatter:on

   void WriteNoCheck(const char *data)
   {
      assert((uint64_t) data % 4 == 0);
      assert((uint64_t) &_blocks[0] % 64 == 0);

      WriteRec<BYTE_COUNT / 4>((uint32_t *) data, _blocks);
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
   inline void ReadRec(uint32_t *result, const uint64_t *blocks)
   {
      constexpr uint32_t meta_block_idx = count>=32 ? 31 : count;

      //@formatter:off
      if constexpr(count>=1)  { result[ 0] = (blocks[ 0] & 0xfffffffe) | (blocks[meta_block_idx] & 0x00000001); }
      if constexpr(count>=2)  { result[ 1] = (blocks[ 1] & 0xfffffffd) | (blocks[meta_block_idx] & 0x00000002); }
      if constexpr(count>=3)  { result[ 2] = (blocks[ 2] & 0xfffffffb) | (blocks[meta_block_idx] & 0x00000004); }
      if constexpr(count>=4)  { result[ 3] = (blocks[ 3] & 0xfffffff7) | (blocks[meta_block_idx] & 0x00000008); }
      if constexpr(count>=5)  { result[ 4] = (blocks[ 4] & 0xffffffef) | (blocks[meta_block_idx] & 0x00000010); }
      if constexpr(count>=6)  { result[ 5] = (blocks[ 5] & 0xffffffdf) | (blocks[meta_block_idx] & 0x00000020); }
      if constexpr(count>=7)  { result[ 6] = (blocks[ 6] & 0xffffffbf) | (blocks[meta_block_idx] & 0x00000040); }
      if constexpr(count>=8)  { result[ 7] = (blocks[ 7] & 0xffffff7f) | (blocks[meta_block_idx] & 0x00000080); }
      if constexpr(count>=9)  { result[ 8] = (blocks[ 8] & 0xfffffeff) | (blocks[meta_block_idx] & 0x00000100); }
      if constexpr(count>=10) { result[ 9] = (blocks[ 9] & 0xfffffdff) | (blocks[meta_block_idx] & 0x00000200); }
      if constexpr(count>=11) { result[10] = (blocks[10] & 0xfffffbff) | (blocks[meta_block_idx] & 0x00000400); }
      if constexpr(count>=12) { result[11] = (blocks[11] & 0xfffff7ff) | (blocks[meta_block_idx] & 0x00000800); }
      if constexpr(count>=13) { result[12] = (blocks[12] & 0xffffefff) | (blocks[meta_block_idx] & 0x00001000); }
      if constexpr(count>=14) { result[13] = (blocks[13] & 0xffffdfff) | (blocks[meta_block_idx] & 0x00002000); }
      if constexpr(count>=15) { result[14] = (blocks[14] & 0xffffbfff) | (blocks[meta_block_idx] & 0x00004000); }
      if constexpr(count>=16) { result[15] = (blocks[15] & 0xffff7fff) | (blocks[meta_block_idx] & 0x00008000); }
      if constexpr(count>=17) { result[16] = (blocks[16] & 0xfffeffff) | (blocks[meta_block_idx] & 0x00010000); }
      if constexpr(count>=18) { result[17] = (blocks[17] & 0xfffdffff) | (blocks[meta_block_idx] & 0x00020000); }
      if constexpr(count>=19) { result[18] = (blocks[18] & 0xfffbffff) | (blocks[meta_block_idx] & 0x00040000); }
      if constexpr(count>=20) { result[19] = (blocks[19] & 0xfff7ffff) | (blocks[meta_block_idx] & 0x00080000); }
      if constexpr(count>=21) { result[20] = (blocks[20] & 0xffefffff) | (blocks[meta_block_idx] & 0x00100000); }
      if constexpr(count>=22) { result[21] = (blocks[21] & 0xffdfffff) | (blocks[meta_block_idx] & 0x00200000); }
      if constexpr(count>=23) { result[22] = (blocks[22] & 0xffbfffff) | (blocks[meta_block_idx] & 0x00400000); }
      if constexpr(count>=24) { result[23] = (blocks[23] & 0xff7fffff) | (blocks[meta_block_idx] & 0x00800000); }
      if constexpr(count>=25) { result[24] = (blocks[24] & 0xfeffffff) | (blocks[meta_block_idx] & 0x01000000); }
      if constexpr(count>=26) { result[25] = (blocks[25] & 0xfdffffff) | (blocks[meta_block_idx] & 0x02000000); }
      if constexpr(count>=27) { result[26] = (blocks[26] & 0xfbffffff) | (blocks[meta_block_idx] & 0x04000000); }
      if constexpr(count>=28) { result[27] = (blocks[27] & 0xf7ffffff) | (blocks[meta_block_idx] & 0x08000000); }
      if constexpr(count>=29) { result[28] = (blocks[28] & 0xefffffff) | (blocks[meta_block_idx] & 0x10000000); }
      if constexpr(count>=30) { result[29] = (blocks[29] & 0xdfffffff) | (blocks[meta_block_idx] & 0x20000000); }
      if constexpr(count>=31) { result[30] = (blocks[30] & 0xbfffffff) | (blocks[meta_block_idx] & 0x40000000); }
      //@formatter:on

      ReadRec<count - 31>(result + 31, blocks + 32);
   }

   //@formatter:off
   template<> inline void ReadRec<0>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-1>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-2>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-3>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-4>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-5>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-6>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-7>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-8>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-9>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-10>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-11>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-12>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-13>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-14>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-15>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-16>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-17>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-18>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-19>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-20>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-21>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-22>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-23>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-24>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-25>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-26>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-27>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-28>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-29>(uint32_t *result, const uint64_t* blocks) {}
   template<> inline void ReadRec<-30>(uint32_t *result, const uint64_t* blocks) {}
   //@formatter:on

   void ReadNoCheck(char *result)
   {
      ReadRec<BYTE_COUNT / 4>((uint32_t *) result, _blocks);
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
      Random ranny;
      std::vector<char> data(entry_size, 'a');
      entries = (InplaceField<entry_size> *) nvm_data.Data();
      for (uint64_t i = 0; i<entry_count; i++) {
         entries[i].Reset();
         entries[i].WriteNoCheck(data.data());
         if (ranny.Rand() % 2 == 0) {
            entries[i].WriteNoCheck(data.data()); // Make it so that the branch is ~50%
         }
      }
   }

   void DoUpdate(const Operation<entry_size> &op)
   {
      entries[op.entry_id].WriteNoCheck((const char *) &op);
      for (uint32_t i = 0; i<sizeof(InplaceField<entry_size>); i += 64) {
         char *addr = (char *) (entries + op.entry_id) + i;
         assert((uint64_t) addr % 64 == 0);
         alex_WriteBack(addr);
      }
      alex_SFence();
   }

   uint64_t ReadSingleResult(Operation<entry_size> &result)
   {
      entries[result.entry_id].ReadNoCheck((char *) &result);
      return result.entry_id;
   }
};
// -------------------------------------------------------------------------------------
}
// -------------------------------------------------------------------------------------

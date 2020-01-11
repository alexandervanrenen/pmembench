#pragma once
// -------------------------------------------------------------------------------------
#include "Common.hpp"
#include "NonVolatileMemory.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <array>
// -------------------------------------------------------------------------------------
namespace v5 {
// -------------------------------------------------------------------------------------
struct Block {
   uint64_t data;

   Block()
           : data(0) {}

   uint32_t GetVersionNoCheck() const { return data >> 62; }
   uint32_t GetOldStateNoCheck() const { return (data >> 31) & 0x7fffffff; }
   uint32_t GetNewStateNoCheck() const { return data & 0x7fffffff; }

   void WriteNoCheck(uint32_t new_state)
   {
      assert((new_state & 0x80000000) == 0);

      uint32_t version = (GetVersionNoCheck() + 1) & 0x3;
      uint32_t old_state = GetNewStateNoCheck();

      AssignNoCheck(version, old_state, new_state);
   }

   uint32_t ReadNoCheck() const
   {
      return GetNewStateNoCheck();
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

private:
   void AssignNoCheck(uint64_t version, uint64_t old_state, uint64_t new_state)
   {
      assert((version & ~0x3) == 0);
      assert((old_state & 0x80000000) == 0);
      assert((new_state & 0x80000000) == 0);

      data = (version << 62) | (old_state << 31) | new_state;
   }
};
static_assert(sizeof(Block) == 8);
// -------------------------------------------------------------------------------------
__m256i constAdd_AVX2 = _mm256_set_epi8(1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0);
__m256i constShuffle_AVX2 = _mm256_set_epi8(15, 11, 10, 9, 0x80, 0x80, 0x80, 0x80, 7, 3, 2, 1, 0x80, 0x80, 0x80, 0x80, 15, 11, 10, 9, 0x80, 0x80, 0x80, 0x80, 7, 3, 2, 1, 0x80, 0x80, 0x80, 0x80);
__m256i constPermute_AVX2 = _mm256_set_epi32(0, 0, 0, 0, 6, 4, 2, 0);
__m128i constShuffle128_AVX2 = _mm_set_epi8(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 12, 8, 4, 0, 0x80);
// -------------------------------------------------------------------------------------
template<uint32_t BYTE_COUNT>
struct InplaceField {

   static const uint32_t BIT_COUNT = BYTE_COUNT * 8;
   static const uint32_t BLOCK_COUNT = (BIT_COUNT + 30) / 31;

   alignas(64) uint64_t blocks[BLOCK_COUNT];

   void Reset()
   {
      memset(blocks, 0, sizeof(uint64_t) * BLOCK_COUNT);
   }

   // ok dann werden die neuen rein-ge-ordert. Dann sieht ja block[0] so aus "oooonnnn"

   void WriteNoCheck(const char *data)
   {
      const uint32_t *values = (const uint32_t *) data;

      // Load the 4 * 32 bit values into 4 * 64 Bit values
      __m128i input32 = _mm_loadu_si128((const __m128i *) values);
      __m256i newValues = _mm256_cvtepu32_epi64(input32);

      // Combine new and old values
      __m256i oldValuesMem = _mm256_loadu_si256((const __m256i *) &blocks[0]);
      __m256i oldValues = _mm256_shuffle_epi8(oldValuesMem, constShuffle_AVX2);
      newValues = _mm256_or_si256(newValues, oldValues);
      newValues = _mm256_add_epi8(newValues, constAdd_AVX2);
      _mm256_storeu_si256((__m256i *) &blocks[0], newValues);

      // Low byes of old value
      __m128i block4 = _mm256_castsi256_si128(_mm256_permutevar8x32_epi32(oldValuesMem, constPermute_AVX2));
      block4 = _mm_shuffle_epi8(block4, constShuffle128_AVX2);
      blocks[4] = _mm_cvtsi128_si64(block4) | ((blocks[4] + 1) & 0xFF);
   }

   static void Dump256(__m256i val)
   {
      char arr[32];
      _mm256_storeu_si256((__m256i_u *) arr, val);
      DumpHexReverse(arr, 32, std::cout);
      std::cout << std::endl;
   }

   static void Dump128(__m128i val)
   {
      char arr[16];
      _mm_storeu_si128((__m128i_u *) arr, val);
      DumpHexReverse(arr, 16, std::cout);
      std::cout << std::endl;
   }

   char *ReadNoCheck()
   {
      Block *blocks_local = (Block *) blocks;
      char *result = (char *) malloc(BYTE_COUNT);
      assert((uint64_t) result % 4 == 0);

      uint32_t buffer = 0;
      uint32_t block_pos = 0;
      uint32_t checker = 0; // opt

      for (uint32_t byte_pos = 0; byte_pos<BYTE_COUNT; byte_pos += 4) {

         if (block_pos % 32 == 0) {
            assert(checker % 31 == 0);
            buffer = blocks_local[block_pos++].ReadNoCheck();
            checker = 0;
         }

         uint32_t cur = blocks_local[block_pos++].ReadNoCheck();
         cur = cur | ((buffer & 0x1) << 31);
         buffer = buffer >> 1;
         memcpy(result + byte_pos, &cur, 4);
         checker++;
      }

      assert(buffer == 0);
      return result;
   }
};
static_assert(sizeof(InplaceField<16>) == 64);
// -------------------------------------------------------------------------------------
template<uint32_t entry_size>
struct InPlaceLikeUpdates {

   static bool CanBeUsed(uint32_t entry_size_param) { return entry_size_param % 16 == 0 && entry_size_param<128; }

   NonVolatileMemory nvm_data;
   uint64_t entry_count;
   InplaceField<entry_size> *entries;

   InPlaceLikeUpdates(const std::string &path, uint64_t entry_count)
           : nvm_data(path + "/inplace_data4_file", sizeof(InplaceField<entry_size>) * entry_count)
             , entry_count(entry_count)
   {
      std::vector<char> data(entry_size, 'a');
      entries = (InplaceField<entry_size> *) nvm_data.Data();
      for (uint64_t i = 0; i<entry_count; i++) {
         entries[i].Reset();
         entries[i].WriteNoCheck(data.data());
      }
   }

   void DoUpdate(uint64_t entry_id, char *new_data)
   {
      entries[entry_id].WriteNoCheck(new_data);
      for (uint32_t i = 0; i<sizeof(InplaceField<entry_size>); i += 64) {
         alex_WriteBack((char *) (entries + entry_id) + i);
      }
      alex_SFence();
   }

   std::vector<char *> PrepareUpdates(uint64_t count)
   {
      Random ranny;
      std::vector<char *> results;
      for (uint64_t i = 0; i<count; i++) {
         results.push_back(CreateAlignedString(ranny, entry_size));
      }
      return results;
   }

   char *CreateResult()
   {
      char *result = (char *) malloc(entry_size * entry_count);
      return result;
   }
};
// -------------------------------------------------------------------------------------
}
// -------------------------------------------------------------------------------------

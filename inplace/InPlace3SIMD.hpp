#pragma once
// -------------------------------------------------------------------------------------
#include "Common.hpp"
#include "NonVolatileMemory.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <array>
// -------------------------------------------------------------------------------------
namespace v3simd {
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
//@formatter:off
const __m256i constShuffle_AVX2 = _mm256_set_epi8(11, 10, 9, 8, 0x80, 0x80, 0x80, 0x80, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80,
                                                  11, 10, 9, 8, 0x80, 0x80, 0x80, 0x80, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80);
const __m256i VERION_MASK = _mm256_set_epi8(
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, ~0x08,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, ~0x04,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, ~0x02,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, ~0x01);
const __m256i VERSION_SET_MASK = _mm256_set_epi8(
        0, 0, 0, 0, 0, 0, 0, 8,
        0, 0, 0, 0, 0, 0, 0, 4,
        0, 0, 0, 0, 0, 0, 0, 2,
        0, 0, 0, 0, 0, 0, 0, 1);
const __m256i VERSION_NOT_MASK = _mm256_set_epi8(
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0);
const std::array<const __m256i*, 2> decision = {&VERSION_NOT_MASK, &VERSION_SET_MASK};
//@formatter:on
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

   void WriteNoCheck(const char *data)
   {
      assert((uint64_t) data % 4 == 0);
      assert((uint64_t) &blocks[0] % 64 == 0);

      uint32_t *input = (uint32_t *) data;
      uint32_t next_version_bit = ((blocks[0] & (1ull << 32)) >> 32) ^0x1;

      // Shift old state
      __m256i oldValuesMem = _mm256_loadu_si256((const __m256i *) &blocks[0]);
      __m256i oldValues = _mm256_shuffle_epi8(oldValuesMem, constShuffle_AVX2);

      __m128i input128 = _mm_loadu_si128((const __m128i *) input);
      __m256i input256 = _mm256_cvtepu32_epi64(input128);

      __m256i version_bit_disabled = _mm256_and_si256(input256, VERION_MASK);
      __m256i new_version_applied = _mm256_or_si256(version_bit_disabled, *decision[next_version_bit]);
      __m256i old_version_added = _mm256_or_si256(new_version_applied, oldValues);
      _mm256_storeu_si256((__m256i *) &blocks[0], old_version_added);

      blocks[4] = (blocks[4] << 32) | ((input[0] & 0x1) | (input[1] & 0x2) | (input[2] & 0x4) | (input[3] & 0x8) | (next_version_bit << 4));
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

   char *ReadNoCheck()
   {
      char *result = (char *) malloc(16);
      assert((uint64_t) result % 4 == 0);
      uint32_t *output = (uint32_t *) result;

      output[0] = (((Block<0> *) &blocks[0])->GetNewStateNoCheck() & ~0x1) | (((Block<4> *) &blocks[4])->GetNewStateNoCheck() & 0x1);
      output[1] = (((Block<1> *) &blocks[1])->GetNewStateNoCheck() & ~0x2) | (((Block<4> *) &blocks[4])->GetNewStateNoCheck() & 0x2);
      output[2] = (((Block<2> *) &blocks[2])->GetNewStateNoCheck() & ~0x4) | (((Block<4> *) &blocks[4])->GetNewStateNoCheck() & 0x4);
      output[3] = (((Block<3> *) &blocks[3])->GetNewStateNoCheck() & ~0x8) | (((Block<4> *) &blocks[4])->GetNewStateNoCheck() & 0x8);

      return result;
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
           : nvm_data(path + "/inplace3_data_file", sizeof(InplaceField<entry_size>) * entry_count)
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
      alex_WriteBack(entries + entry_id);
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
      for (uint64_t i = 0; i<entry_count; i++) {
         char *entry_as_string = entries[i].ReadNoCheck();
         memcpy(result + i * entry_size, entry_as_string, entry_size);
         free(entry_as_string);
      }
      return result;
   }
};
// -------------------------------------------------------------------------------------
}
// -------------------------------------------------------------------------------------

#pragma once
// -------------------------------------------------------------------------------------
#include "Common.hpp"
#include "NonVolatileMemory.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <array>
// -------------------------------------------------------------------------------------
namespace v2simd {
// -------------------------------------------------------------------------------------
__m256i constAdd = _mm256_set1_epi64x(0x4000000000000000L);
__m256i constAnd1 = _mm256_set1_epi64x(0xC000000000000000L);
__m256i constAnd2 = _mm256_set1_epi64x(0x3FFFFFFFFFFFFFFFL);
__m128i constAnd3 = _mm_set1_epi32(0x7FFFFFFF);
__m128i constGatherIndex = _mm_set_epi32(6, 4, 2, 0);
__m128i constAnd = _mm_set1_epi32(0x7FFFFFFF);
// -------------------------------------------------------------------------------------
template<uint32_t BYTE_COUNT>
struct InplaceField {
   static constexpr uint32_t BIT_COUNT = BYTE_COUNT * 8;
   static constexpr uint32_t BLOCK_COUNT = (BIT_COUNT + 30) / 31;
   static constexpr uint32_t META_BLOCK_COUNT = (BYTE_COUNT + 111) / 112; // For every 112 input bytes we need one meta block (8byte)

   alignas(64) uint64_t _blocks[BLOCK_COUNT];

   void Reset()
   {
      memset(_blocks, 0, sizeof(uint64_t) * BLOCK_COUNT);
   }

   template<uint32_t META>
   void WriteRec(const uint32_t *values, uint64_t *blocks)
   {
      uint32_t currentHighBits = 0;

      constexpr uint32_t REMAINING_BYTE = BYTE_COUNT - (META * 112);
      constexpr uint32_t ITERATION_COUNT = (META == META_BLOCK_COUNT - 1) ? REMAINING_BYTE / 16 : 7;

      for (uint32_t i = 0; i<ITERATION_COUNT; i++) {
         // 4 * 32-Bit input values into SSE register
         __m128i input32 = _mm_loadu_si128((const __m128i *) &values[i * 4]);

         // The four 31-bit values
         __m256i input64 = _mm256_cvtepu32_epi64(_mm_and_si128(input32, constAnd3));      // 4 * 64-Bit input value
         __m256i block1_4 = _mm256_loadu_si256((const __m256i *) &blocks[1 + i * 4]);
         __m256i newVersionSimd = _mm256_and_si256(_mm256_add_epi64(block1_4, constAdd), constAnd1);
         __m256i newPayloadSimd = _mm256_or_si256(_mm256_and_si256(_mm256_slli_epi64(block1_4, 31), constAnd2), input64);
         _mm256_storeu_si256((__m256i *) &blocks[1 + i * 4], _mm256_or_si256(newVersionSimd, newPayloadSimd));
         currentHighBits = (currentHighBits << 4) | _mm_movemask_ps(_mm_castsi128_ps(input32));
      }

      // The four high bits
      uint64_t block0 = blocks[0];
      uint64_t newVersion = (block0 + 0x4000000000000000L) & 0xC000000000000000L;      // 2 bits for version
      uint64_t newPayload = ((block0 << 31) & 0x3FFFFFFFFFFFFFFFL) | currentHighBits;   // Payload with new and old value

      blocks[0] = newVersion | newPayload;

      WriteRec<META + 1>(values + 28, blocks + 29);
   }

   template<>
   void WriteRec<META_BLOCK_COUNT>(const uint32_t *, uint64_t *) {}

   void WriteNoCheck(const char *data)
   {
      const uint32_t *values = (const uint32_t *) data;
      WriteRec<0>(values, _blocks);
   }

   template<uint32_t META>
   void ReadRec(const char *result, uint64_t *blocks)
   {
      constexpr uint32_t REMAINING_BYTE = BYTE_COUNT - (META * 112);
      constexpr uint32_t ITERATION_COUNT = (META == META_BLOCK_COUNT - 1) ? REMAINING_BYTE / 16 : 7;

      uint32_t high_bits = blocks[0];

      for (int32_t i = ITERATION_COUNT - 1; i>=0; i--) {
         // Gather 4 * 32-Bit Values out of 64-Bit Array (gather lower 32 bits of each 64 bit value)
         __m128i values = _mm_i32gather_epi32((const int *) &blocks[1 + i * 4], constGatherIndex, 4);

         // Remove highest bit, so we have 4 * 31 bi values
         values = _mm_and_si128(values, constAnd);

         // Store the four high bits (lowest 4 bits in block[0]) to bit position 0, 8, 16, 24 (== align to byt boundary)
         __m128i highBitValue = _mm_cvtsi64_si128(_pdep_u64(high_bits, 0x01010101));
         high_bits = high_bits >> 4;

         // Convert from 8 bit values to 32 bit values and shift left to the highest position
         highBitValue = _mm_cvtepu8_epi32(highBitValue);
         highBitValue = _mm_slli_epi32(highBitValue, 31);

         // OR together
         values = _mm_or_si128(values, highBitValue);

         // Store result
         _mm_storeu_si128((__m128i *) (result + i * 16), values);
      }

      ReadRec<META + 1>(result + 112, blocks + 29);
   }

   template<>
   void ReadRec<META_BLOCK_COUNT>(const char *result, uint64_t *blocks) {}

   void ReadNoCheck(char *result)
   {
      ReadRec<0>(result, _blocks);
   }
};
// -------------------------------------------------------------------------------------
template<uint32_t entry_size>
struct InPlaceLikeUpdates {

   static bool CanBeUsed(uint32_t entry_size_param) { return entry_size_param % 16 == 0 && entry_size_param<128; }

   NonVolatileMemory nvm_data;
   uint64_t entry_count;
   InplaceField<entry_size> *entries;

   InPlaceLikeUpdates(const std::string &path, uint64_t entry_count)
           : nvm_data(path + "/inplace_file", sizeof(InplaceField<entry_size>) * entry_count)
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

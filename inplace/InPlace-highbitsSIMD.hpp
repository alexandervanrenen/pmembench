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
__m256i constAdd = _mm256_set1_epi64x(0x4000000000000000L);
__m256i constAnd1 = _mm256_set1_epi64x(0xC000000000000000L);
__m256i constAnd2 = _mm256_set1_epi64x(0x3FFFFFFFFFFFFFFFL);
__m128i constAnd3 = _mm_set1_epi32(0x7FFFFFFF);
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

   void WriteNoCheck(const char *data)
   {
      const uint32_t *values = (const uint32_t *) data;

      uint32_t currentHighBits = 0;

      for (uint32_t i = 0; i<BYTE_COUNT / 16; i++) {
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
         char *entry_as_string = entries[i].ReadNoCheck();
         memcpy((char *) &result[i], entry_as_string, entry_size);
         free(entry_as_string);
      }
   }
};
// -------------------------------------------------------------------------------------
}
// -------------------------------------------------------------------------------------

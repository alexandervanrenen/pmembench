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

   void WriteNoCheck(const char *data)
   {
      assert((uint64_t) data % 4 == 0);
      assert((uint64_t) &blocks[0] % 64 == 0);

      static constexpr std::array<uint32_t, 4> version_bit = {1, 1, 0, 0};
      static constexpr std::array<uint32_t, 2> masks = {0, ~uint32_t(0)};

      uint32_t *input = (uint32_t *) data;

      uint32_t current_version_bit = (blocks[0] & 0x10) >> 4;
      uint32_t next_version_bit = version_bit[current_version_bit];
      uint32_t mask = masks[next_version_bit];

      blocks[0] = (blocks[0] << 32) | ((input[0] & ~0x1) | (0x1 & mask));
      blocks[1] = (blocks[1] << 32) | ((input[1] & ~0x2) | (0x2 & mask));
      blocks[2] = (blocks[2] << 32) | ((input[2] & ~0x4) | (0x4 & mask));
      blocks[3] = (blocks[3] << 32) | ((input[3] & ~0x8) | (0x8 & mask));
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

   void ReadNoCheck(char *result)
   {
      uint32_t *output = (uint32_t *) result;

      output[0] = (((Block<0> *) &blocks[0])->GetNewStateNoCheck() & ~0x1) | (((Block<4> *) &blocks[4])->GetNewStateNoCheck() & 0x1);
      output[1] = (((Block<1> *) &blocks[1])->GetNewStateNoCheck() & ~0x2) | (((Block<4> *) &blocks[4])->GetNewStateNoCheck() & 0x2);
      output[2] = (((Block<2> *) &blocks[2])->GetNewStateNoCheck() & ~0x4) | (((Block<4> *) &blocks[4])->GetNewStateNoCheck() & 0x4);
      output[3] = (((Block<3> *) &blocks[3])->GetNewStateNoCheck() & ~0x8) | (((Block<4> *) &blocks[4])->GetNewStateNoCheck() & 0x8);
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

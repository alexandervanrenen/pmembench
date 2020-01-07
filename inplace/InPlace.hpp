#pragma once
// -------------------------------------------------------------------------------------
#include "Common.hpp"
#include "NonVolatileMemory.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <array>
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
template<uint32_t BYTE_COUNT>
struct InplaceField {
   static const uint32_t BIT_COUNT = BYTE_COUNT * 8;
   static const uint32_t BLOCK_COUNT = (BIT_COUNT + 30) / 31;
   static_assert(BYTE_COUNT>8, "Use normal uint64_t.");

   std::array<Block, BLOCK_COUNT> blocks;

   void Print(std::ostream &out)
   {
      for (auto &block : blocks) {
         out << block << std::endl;
      }
   }

   void Reset()
   {
      memset(blocks.data(), 0, sizeof(Block) * BLOCK_COUNT);
   }

   void WriteNoCheck(const char *data)
   {
      assert((uint64_t) data % 4 == 0);
      uint32_t buffer = 0;
      uint32_t block_pos = 1; // Block zero is meta block
      uint32_t checker = 0;

      for (uint32_t byte_pos = 0; byte_pos<BYTE_COUNT; byte_pos += 4) {
         uint32_t cur = *Cast<const uint32_t *>(data + byte_pos);
         buffer = (buffer | (cur & 0x80000000)) >> 1;
         blocks[block_pos++].WriteNoCheck(cur & 0x7fffffff);
         checker++;

         if (block_pos % 32 == 0) {
            assert(checker == 31);
            blocks[block_pos - 32].WriteNoCheck(buffer);
            block_pos++;
            buffer = 0;
            checker = 0;
         }
      }

      if (block_pos % 32 != 0) {
         assert(checker>0);
         uint32_t meta_block_pos = block_pos - block_pos % 32;
         blocks[meta_block_pos].WriteNoCheck(buffer >> (32 - block_pos));
         buffer = 0;
         checker = 0;
      }
   }

   char *ReadNoCheck()
   {
      char *result = (char *) malloc(BYTE_COUNT);
      assert((uint64_t) result % 4 == 0);

      uint32_t buffer = 0;
      uint32_t block_pos = 0;
      uint32_t checker = 0; // opt

      for (uint32_t byte_pos = 0; byte_pos<BYTE_COUNT; byte_pos += 4) {

         if (block_pos % 32 == 0) {
            assert(checker % 31 == 0);
            buffer = blocks[block_pos++].ReadNoCheck();
            checker = 0;
         }

         uint32_t cur = blocks[block_pos++].ReadNoCheck();
         cur = cur | ((buffer & 0x1) << 31);
         buffer = buffer >> 1;
         memcpy(result + byte_pos, &cur, 4);
         checker++;
      }

      assert(buffer == 0);
      return result;
   }
};
// -------------------------------------------------------------------------------------
template<uint32_t BYTE_COUNT>
void TestInPlaceUpdates()
{
   Random ranny;
   InplaceField<BYTE_COUNT> field;

   for (uint32_t i = 0; i<10000; i++) {
      field.Reset();
      char *input = CreateAlignedString(ranny, BYTE_COUNT);
      field.WriteNoCheck(input);
      char *output = field.ReadNoCheck();

      for (uint32_t i = 0; i<BYTE_COUNT; i++) {
         if (input[i] != output[i]) {
            std::cout << i << ": ";
            DumpHex(input + i, 1, std::cout);
            std::cout << " vs ";
            DumpHex(output + i, 1, std::cout);
            std::cout << std::endl;
            throw;
         }
      }

      free(output);
      free(input);
   }
   std::cout << "all good for " << BYTE_COUNT << std::endl;
}
// -------------------------------------------------------------------------------------
void TestSomeInPlaceUpdateConfigurations()
{
   TestInPlaceUpdates<16>();
   TestInPlaceUpdates<20>();
   TestInPlaceUpdates<64>();
   TestInPlaceUpdates<1000>();
   TestInPlaceUpdates<10000>();
}
// -------------------------------------------------------------------------------------
template<uint32_t entry_size>
struct InPlaceLikeUpdates {
   NonVolatileMemory nvm_data;

   InPlaceLikeUpdates(const std::string &path)
           : nvm_data(path + "/data_file", sizeof(InplaceField<entry_size>))
   {
      std::cout << "size: " << sizeof(InplaceField<entry_size>) << std::endl;
      std::cout << "BLOCK_COUNT: " << InplaceField<entry_size>::BLOCK_COUNT << std::endl;
   }

   void DoUpdate(char *new_data)
   {
      std::cout << "update to ";
      DumpHex(new_data, entry_size, std::cout);
      std::cout << std::endl;
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
};
// -------------------------------------------------------------------------------------

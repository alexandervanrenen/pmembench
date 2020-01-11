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

   void WriteNoCheck(uint32_t new_state)
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
struct InplaceField16 {

   alignas(64)
   Block<0> b0;
   Block<1> b1;
   Block<2> b2;
   Block<3> b3;
   Block<4> b4;

   void Print(std::ostream &out)
   {
      out << b0 << std::endl;
      out << b1 << std::endl;
      out << b2 << std::endl;
      out << b3 << std::endl;
      out << b4 << std::endl;
   }

   void Reset()
   {
      b0.data = 0;
      b1.data = 0;
      b2.data = 0;
      b3.data = 0;
      b4.data = 0;
   }

   void WriteNoCheck(const char *data)
   {
      static std::array<uint32_t, 4> VersionBit = {1, 1, 0, 0};

      assert((uint64_t) data % 4 == 0);
      assert((uint64_t) &b0 % 64 == 0);

      uint32_t *input = (uint32_t *) data;

      uint32_t next_version_bit = VersionBit[b0.GetVersionNoCheck()];
      if (next_version_bit) {
         //@formatter:off
         b1.WriteNoCheck( input[0] | 0x02);
         b2.WriteNoCheck( input[1] | 0x04);
         b3.WriteNoCheck( input[2] | 0x08);
         b4.WriteNoCheck( input[3] | 0x10);
         b0.WriteNoCheck( (input[0] & 0x02)
                          | (input[1] & 0x04)
                          | (input[2] & 0x08)
                          | (input[3] & 0x10)
                          | 0x1);
         //@formatter:on
      } else {
         //@formatter:off
         b1.WriteNoCheck( input[0] & ~0x02);
         b2.WriteNoCheck( input[1] & ~0x04);
         b3.WriteNoCheck( input[2] & ~0x08);
         b4.WriteNoCheck( input[3] & ~0x10);
         b0.WriteNoCheck( (input[0] & 0x02)
                          | (input[1] & 0x04)
                          | (input[2] & 0x08)
                          | (input[3] & 0x10)
                          | 0x0);
         //@formatter:on
      }
   }

   char *ReadNoCheck()
   {
      char *result = (char *) malloc(16);
      assert((uint64_t) result % 4 == 0);
      uint32_t *output = (uint32_t *) result;

      output[0] = (b1.GetNewStateNoCheck() & ~0x02) | (b0.GetNewStateNoCheck() & 0x02);
      output[1] = (b2.GetNewStateNoCheck() & ~0x04) | (b0.GetNewStateNoCheck() & 0x04);
      output[2] = (b3.GetNewStateNoCheck() & ~0x08) | (b0.GetNewStateNoCheck() & 0x08);
      output[3] = (b4.GetNewStateNoCheck() & ~0x10) | (b0.GetNewStateNoCheck() & 0x10);

      return result;
   }
};
// -------------------------------------------------------------------------------------
void TestInPlaceUpdates()
{
   Random ranny;
   InplaceField16 field;

   for (uint32_t i = 0; i<10000; i++) {
      field.Reset();
      char *input = CreateAlignedString(ranny, 16);
      field.WriteNoCheck(input);
      char *output = field.ReadNoCheck();

      for (uint32_t i = 0; i<16; i++) {
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
   std::cout << "all good for " << 16 << std::endl;
}
// -------------------------------------------------------------------------------------
void TestSomeInPlaceUpdateConfigurations()
{
   TestInPlaceUpdates();
}
// -------------------------------------------------------------------------------------
template<uint32_t entry_size>
struct InPlaceLikeUpdates {

   static bool CanBeUsed(uint32_t entry_size_param) { return entry_size_param == 16; }

   NonVolatileMemory nvm_data;
   uint64_t entry_count;
   InplaceField16 *entries;

   InPlaceLikeUpdates(const std::string &path, uint64_t entry_count)
           : nvm_data(path + "/inplace3_data_file", sizeof(InplaceField16) * entry_count)
             , entry_count(entry_count)
   {
      std::vector<char> data(entry_size, 'a');
      entries = (InplaceField16 *) nvm_data.Data();
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

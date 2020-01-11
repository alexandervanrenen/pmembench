#include <iostream>
#include <vector>
#include <string>
#include <array>
// -------------------------------------------------------------------------------------
// Based on: https://en.wikipedia.org/wiki/Xorshift
class Random {
public:
   explicit Random(uint64_t seed = 2305843008139952128ull) // The 8th perfect number found 1772 by Euler with <3
           : seed(seed)
   {
   }

   uint64_t Rand()
   {
      seed ^= (seed << 13);
      seed ^= (seed >> 7);
      return (seed ^= (seed << 17));
   }

   uint64_t seed;
};
// -------------------------------------------------------------------------------------
char *CreateAlignedString(Random &ranny, uint32_t len)
{
   char *data = (char *) malloc(len + 1);
   assert((uint64_t) data % 4 == 0);

   for (uint32_t i = 0; i<len; i++) {
      data[i] = ranny.Rand() % 256;
   }
   data[len] = '\0';

   return data;
}
// -------------------------------------------------------------------------------------
void DumpHex(const void *data_in, uint32_t size, std::ostream &os)
{
   char buffer[16];

   const char *data = reinterpret_cast<const char *>(data_in);
   for (uint32_t i = 0; i<size; i++) {
      sprintf(buffer, "%02hhx", data[i]);
      os << buffer[0] << buffer[1] << " ";
   }
}
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
template<uint32_t BYTE_COUNT>
void TestInPlaceUpdates()
{
   Random ranny;
   InplaceField16 field;

   for (uint32_t i = 0; i<10000; i++) {
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
   //   TestInPlaceUpdates<20>();
   //   TestInPlaceUpdates<64>();
   //   TestInPlaceUpdates<1000>();
   //   TestInPlaceUpdates<10000>();
}
// -------------------------------------------------------------------------------------
int main()
{
   TestInPlaceUpdates<16>();
   return 0;
}
// -------------------------------------------------------------------------------------

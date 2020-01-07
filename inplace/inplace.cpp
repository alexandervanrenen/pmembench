#include <iostream>
#include <vector>
#include <string>
// -------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------
void DumpHex(const void *data_in, uint32_t size, ostream &os)
{
   char buffer[16];

   const char *data = reinterpret_cast<const char *>(data_in);
   for (uint32_t i = 0; i<size; i++) {
      sprintf(buffer, "%02hhx", data[i]);
      os << buffer[0] << buffer[1] << " ";
   }
}
// -------------------------------------------------------------------------------------
struct Block {
   uint64_t data;

   Block()
           : data(0) {}

   uint32_t GetVersionNoCheck() const { return data >> 62; }
   uint32_t GetOldStateNoCheck() const { return (data >> 31) & 0x7fffffff; }
   uint32_t GetNewStateNoCheck() const { return data & 0x7fffffff; }

   void UpdateNoCheck(uint32_t new_state)
   {
      assert((new_state & 0x80000000) == 0);

      uint32_t version = (GetVersionNoCheck() + 1) & 0x3;
      uint32_t old_state = GetNewStateNoCheck();

      AssignNoCheck(version, old_state, new_state);
   }

   friend ostream &operator<<(ostream &os, const Block &b)
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
int main()
{
   uint32_t c = 1;
   Block b;
   cout << b << endl;
   b.UpdateNoCheck(c++);
   cout << b << endl;
   b.UpdateNoCheck(c++);
   cout << b << endl;
   b.UpdateNoCheck(c++);
   cout << b << endl;
   b.UpdateNoCheck(c++);
   cout << b << endl;
   b.UpdateNoCheck(c++);
   cout << b << endl;
}
// -------------------------------------------------------------------------------------

#include <iostream>
#include <vector>
#include <string>
// -------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------
struct Log {
   vector<uint8_t> log;
   uint64_t log_offset;
   uint64_t log_read_offset;

   Log(uint64_t size)
           : log(size, 0)
             , log_offset(0)
             , log_read_offset(0)
   {
      memset((char *) &log[0], '\0', size);
   }

   void Write(const char *data, uint64_t len)
   {
      assert(log_offset + len<log.size());
      assert(len>0 && len % 8 == 0);

      // Write length of log entry
      uint64_t log_offset_start = log_offset;
      *((uint64_t *) &log[log_offset]) = len | 0x1;
      log_offset += 8;

      // Write payload of log entry
      uint64_t buffer = 0;
      uint64_t checker = 0;

      uint32_t i = 0;
      for (i = 0; i<len; i += 8) {
         uint64_t cur = *(const uint64_t *) &data[i];

         buffer = (buffer << 1) | (cur & 0x1);
         cur = (cur | 0x1);
         *((uint64_t *) &log[log_offset]) = cur;
         log_offset += 8;
         //         cout << i << ": " << buffer << endl;

         // Flush buffer every 63 8-byte blocks
         checker++;
         if (i>0 && (i + 8) % 504 == 0) {
            cout << "flush checker" << endl;
            assert(checker == 63);
            buffer = (buffer << 1) | 0x1;
            *((uint64_t *) &log[log_offset]) = buffer;
            log_offset += 8;
            checker = 0;
            buffer = 0;
         }
      }

      if (i % 504 != 0) {
         assert(checker>0);
         cout << "flush tail checker: " << buffer << " @" << log_offset << endl;

         buffer = (buffer << 1) | 0x1;
         *((uint64_t *) &log[log_offset]) = buffer;
         log_offset += 8;
      }
   }

   vector<char> ReadNextEntry() // Read code is only to verify correctnes
   {
      // Read length
      uint64_t len = *((uint64_t *) &log[0]) & ~(0x1ull);
      log_read_offset += 8;

      cout << "got len: " << len << endl;

      uint64_t checker = 0;

      vector<char> result(len);
      uint32_t i = 0;
      for (; i<len; i += 8) {
         *((uint64_t *) &result[i]) = *((uint64_t *) &log[log_read_offset]);
         *((uint64_t *) &result[i]) &= ~(0x1ull);
         log_read_offset += 8;
         checker++;

         if (i>0 && (i + 8) % 504 == 0) {
            assert(checker == 63);
            checker = 0;
            cout << "apply checker" << endl;

            uint64_t buffer = *((uint64_t *) &log[log_read_offset]) >> 1;
            log_read_offset += 8;
            for (uint32_t c = 0; c<63; c++) {
               *((uint64_t *) &result[i - c * 8]) |= buffer & 0x1;
               buffer = buffer >> 1;
            }
         }
      }

      if (i % 504 != 0) {
         i -= 8;
         assert(checker>0);

         uint64_t buffer = *((uint64_t *) &log[log_read_offset]) >> 1;
         cout << "apply tail checker " << buffer << " @" << log_read_offset << endl;

         log_read_offset += 8;
         for (uint32_t c = 0; c<checker; c++) {
            *((uint64_t *) &result[i - c * 8]) |= buffer & 0x1;
            buffer = buffer >> 1;
         }
         checker = 0;
      }

      return result;
   }
};
// -------------------------------------------------------------------------------------
int main()
{
   Log log(1e9);

   {
      //   string test = "Whats up N York?";
      string test = "Hello_my_world! ";
      for (uint32_t i = 0; i<7; i++) {
         test += test;
      }
      assert(test.size() == 16 * 128);
      log.Write(test.data(), 5 * 8);

      vector<char> result = log.ReadNextEntry();
      cout << "got:" << endl;
      for (auto iter : result) {
         cout << iter;
      }
      cout << endl;
   }

   {
      //   string test = "Whats up N York?";
      string test = "Hello_my_world! ";
      for (uint32_t i = 0; i<7; i++) {
         test += test;
      }
      assert(test.size() == 16 * 128);
      log.Write(test.data(), 5 * 8);

      vector<char> result = log.ReadNextEntry();
      cout << "got:" << endl;
      for (auto iter : result) {
         cout << iter;
      }
      cout << endl;
   }
}
// -------------------------------------------------------------------------------------

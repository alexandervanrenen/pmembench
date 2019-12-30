#include <libpmemlog.h>

struct LogWriterPMemLib {

   struct Entry {
      ub8 payload_size; // header
      ub1 data[];
   };

   ub8 next_free;
   PMEMlogpool *log;

   LogWriterPMemLib(const std::string &file_name, ub8 file_size)
   {
      system(("rm -f " + file_name + "_pmemlib").c_str());
      log = pmemlog_create((file_name + "_pmemlib").c_str(), file_size, 0666);
      if (log == nullptr) {

      }

      next_free = 0;
   }
   ub8 AddLogEntry(const Entry &entry)
   {
      int res = pmemlog_append(log, &entry, entry.payload_size + 8); // We need to writte the size of the log entry, because they dont
      if (res<0) {
         cout << "error writing to the pmem log" << endl;
         assert(false);
         throw;
      }

      return 11111111;
   }

   static vector<Entry *> CreateRandomEntries(vector <ub1> &memory, ub4 min_size, ub4 max_size, ub8 log_payload_size, Random &ranny)
   {
      ub8 current = 0;
      ub8 used_size = 0;

      vector<Entry *>entries;
      while (used_size<log_payload_size) {
         ub8 payload_size = ((ranny.Rand() % (max_size - min_size + 1)) + min_size) * 8;
         assert(current + payload_size + 16<memory.size());
         Entry *entry = (Entry *) &memory[current];
         entry->payload_size = payload_size;
         entries.push_back(entry);
         current += payload_size + 16;
         used_size += payload_size;
      }
      return entries;
   }
};
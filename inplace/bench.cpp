#include "InPlace-generic.hpp"
#include "InPlace-highbits.hpp"
#include "InPlace-highbitsSIMD.hpp"
#include "InPlace-shifting.hpp"
#include "InPlace-shiftingSIMD.hpp"
#include "LogBased.hpp"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <chrono>
// -------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------
#ifndef ENTRY_SIZE
#error Please define ENTRY_SIZE
#define ENTRY_SIZE 4
#endif
// -------------------------------------------------------------------------------------
uint64_t UPDATE_COUNT;
uint64_t ENTRY_COUNT;
string NVM_PATH;
vector<UpdateOperation<ENTRY_SIZE>> log_result;
// -------------------------------------------------------------------------------------
template<class COMPETITOR>
void RunExperiment(const std::string &competitor_name, vector<UpdateOperation<ENTRY_SIZE>> &updates)
{
   COMPETITOR competitor(NVM_PATH, ENTRY_COUNT);
   Random ranny;

   uint64_t updates_per_second;
   uint64_t seq_reads_per_second;
   uint64_t rand_reads_per_second;

   {
      auto begin_ts = chrono::high_resolution_clock::now();
      for (uint64_t u = 0; u<UPDATE_COUNT; u++) {
         competitor.DoUpdate(updates[u]);
      }
      auto end_ts = chrono::high_resolution_clock::now();
      uint64_t ns = chrono::duration_cast<chrono::nanoseconds>(end_ts - begin_ts).count();
      updates_per_second = (UPDATE_COUNT * 1e9) / ns;
   }

   {
      auto begin_ts = chrono::high_resolution_clock::now();
      vector<UpdateOperation<ENTRY_SIZE>> result(ENTRY_COUNT);
      competitor.ReadResult(result);
      auto end_ts = chrono::high_resolution_clock::now();
      uint64_t ns = chrono::duration_cast<chrono::nanoseconds>(end_ts - begin_ts).count();
      seq_reads_per_second = (ENTRY_COUNT * 1e9) / ns;
   }

   {
      auto begin_ts = chrono::high_resolution_clock::now();
      vector<UpdateOperation<ENTRY_SIZE>> result = updates;
      for (uint64_t u = 0; u<UPDATE_COUNT; u++) {
         competitor.ReadSingleResult(result[u]);
      }
      auto end_ts = chrono::high_resolution_clock::now();
      uint64_t ns = chrono::duration_cast<chrono::nanoseconds>(end_ts - begin_ts).count();
      rand_reads_per_second = (UPDATE_COUNT * 1e9) / ns;
   }

   //@formatter:off
   cout << "res:"
        << " technique: " << competitor_name
        << " entry_size: " << ENTRY_SIZE
        << " updates_per_second(M): " << updates_per_second  / 1000 / 1000.0
        << " seq_read_per_second(M): " << seq_reads_per_second  / 1000 / 1000.0
        << " rand_read_per_second(M): " << rand_reads_per_second  / 1000 / 1000.0
        << endl;
   //@formatter:on

   const bool VALIDATE = true;
   if (VALIDATE) {
      vector<UpdateOperation<ENTRY_SIZE>> result(ENTRY_COUNT);
      competitor.ReadResult(result);
      if (competitor_name == "log-based") {
         log_result = result;
         return;
      }
      if (log_result.size()>0) {
         if (log_result.size() != result.size()) {
            cout << "validation failed !!" << endl;
            throw;
         }
         for (uint64_t i = 0; i<log_result.size(); i++) {
            if (log_result[i] != result[i]) {
               cout << "validation failed !!" << endl;
               throw;
            }
         }
         cout << "ok" << endl;
      }
   }
}
// -------------------------------------------------------------------------------------
vector<UpdateOperation<ENTRY_SIZE>> PrepareUpdates()
{
   Random ranny;
   std::vector<UpdateOperation<ENTRY_SIZE>> results(UPDATE_COUNT);
   for (uint64_t i = 0; i<UPDATE_COUNT; i++) {
      results[i].entry_id = ranny.Rand() % ENTRY_COUNT;
      for (uint32_t j = 0; j<ENTRY_SIZE - 8; j += 8) {
         *(uint64_t *) &results[i].data[j] = ranny.Rand();
      }
   }

   return results;
}
// -------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
   if (argc != 4) {
      cout << "usage: " << argv[0] << " update_count entry_count nvm_path" << endl;
      throw;
   }

   if (ENTRY_SIZE % 4 != 0) {
      cout << "ENTRY_SIZE needs to be a multiple of 4 for now" << endl;
      throw;
   }

   // Config
   ENTRY_COUNT = atof(argv[1]);
   UPDATE_COUNT = atof(argv[2]);
   NVM_PATH = argv[3]; // Path to the nvm folder

   cout << "Config" << endl;
   cout << "------" << endl;
   cout << "entry_size         " << ENTRY_SIZE << endl;
   cout << "entry_count(M)     " << ENTRY_COUNT / 1000 / 1000.0 << endl;
   cout << "needed_data(GB)    " << ENTRY_COUNT * ENTRY_SIZE / 1000 / 1000 / 1000.0 << endl;
   cout << "actual_data(GB)    " << ENTRY_COUNT * sizeof(UpdateOperation<ENTRY_SIZE>) / 1000 / 1000 / 1000.0 << endl;
   cout << "update_count(M)    " << UPDATE_COUNT / 1000 / 1000.0 << endl;
   cout << "nvm_path           " << NVM_PATH << endl;
   cout << "------" << endl;

   auto update_vec = PrepareUpdates();

   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   CPU_SET(0, &cpuset);
   pthread_t currentThread = pthread_self();
   if (pthread_setaffinity_np(currentThread, sizeof(cpu_set_t), &cpuset) != 0)
      throw;

   // Run Experiments
   RunExperiment<LogBasedUpdates<ENTRY_SIZE>>("log-based", update_vec);
   //   RunExperiment<v1::InPlaceLikeUpdates<ENTRY_SIZE>>("generic-loop", update_vec);
   //   RunExperiment<v2::InPlaceLikeUpdates<ENTRY_SIZE>>("high-bits", update_vec);
   RunExperiment<v2simd::InPlaceLikeUpdates<ENTRY_SIZE>>("high-bits-simd(stef)", update_vec);
   RunExperiment<v3::InPlaceLikeUpdates<ENTRY_SIZE>>("moving-version", update_vec);
   //   RunExperiment<v2simd::InPlaceLikeUpdates<ENTRY_SIZE>>("high-bits-simd(stef)", update_vec);

   // TODO: CoW !!!!
   //      CowBasedUpdates<ENTRY_SIZE> cow_based(NVM_PATH);
   //      RunExperiment("cow", log_based, strings);

   cout << "done 1" << endl;
   return 0;
}
// -------------------------------------------------------------------------------------

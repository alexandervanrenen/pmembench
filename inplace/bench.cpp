#include "InPlace-highBit.hpp"
#include "InPlace-sliding.hpp"
#include "LogBased.hpp"
#include "CowBased.hpp"
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
uint64_t OPERATION_COUNT;
uint64_t ENTRY_COUNT;
string NVM_PATH;
vector<Operation<ENTRY_SIZE>> log_result;
const bool VALIDATE = false;
bool SEQUENTIAL;
// -------------------------------------------------------------------------------------
vector<Operation<ENTRY_SIZE>> PrepareSeqentialOperations()
{
   std::vector<Operation<ENTRY_SIZE>> results(OPERATION_COUNT);
   for (uint64_t i = 0; i<OPERATION_COUNT; i++) {
      results[i].entry_id = (i + 1) % OPERATION_COUNT;
      memset(results[i].data.data(), (char) i, results[i].data.size());
   }

   return results;
}
// -------------------------------------------------------------------------------------
vector<Operation<ENTRY_SIZE>> PrepareRandomOperations()
{
   // Init
   uint64_t *helper = new uint64_t[OPERATION_COUNT];
   for (uint64_t i = 0; i<OPERATION_COUNT; i++) {
      helper[i] = i;
   }

   // Shuffle
   Random ranny;
   for (uint64_t i = 0; i<OPERATION_COUNT; i++) {
      swap(helper[i], helper[ranny.Rand() % OPERATION_COUNT]);
   }

   // Assign
   std::vector<Operation<ENTRY_SIZE>> results;
   results.reserve(OPERATION_COUNT);
   for (uint64_t i = 0; i<OPERATION_COUNT; i++) {
      results[helper[i]].entry_id = helper[(i + 1) % OPERATION_COUNT];
      memset(results[i].data.data(), (char) i, results[i].data.size());
   }

   return results;
}
// -------------------------------------------------------------------------------------
template<class COMPETITOR>
void RunExperiment(const std::string &competitor_name, vector<Operation<ENTRY_SIZE>> &operations)
{
   vector<Operation<ENTRY_SIZE>> expected;
   if (VALIDATE) {
      expected = operations;
   }

   COMPETITOR competitor(NVM_PATH, ENTRY_COUNT);

   uint64_t updates_per_second = 0;
   uint64_t reads_per_second = 0;
   uint64_t dep_reads_per_second = 0;

   {
      auto begin_ts = chrono::high_resolution_clock::now();
      for (uint64_t u = 0; u<OPERATION_COUNT; u++) {
         competitor.DoUpdate(operations[u]);
      }
      auto end_ts = chrono::high_resolution_clock::now();
      uint64_t ns = chrono::duration_cast<chrono::nanoseconds>(end_ts - begin_ts).count();
      updates_per_second = (OPERATION_COUNT * 1e9) / ns;
   }

   {
      auto begin_ts = chrono::high_resolution_clock::now();
      uint64_t next_id = 0;
      for (uint64_t u = 0; u<OPERATION_COUNT; u++) {
         next_id = competitor.ReadSingleResult(operations[next_id]);
      }
      auto end_ts = chrono::high_resolution_clock::now();
      uint64_t ns = chrono::duration_cast<chrono::nanoseconds>(end_ts - begin_ts).count();
      dep_reads_per_second = (OPERATION_COUNT * 1e9) / ns;
   }

   {
      auto begin_ts = chrono::high_resolution_clock::now();
      for (uint64_t u = 0; u<OPERATION_COUNT; u++) {
         competitor.ReadSingleResult(operations[u]);
      }
      auto end_ts = chrono::high_resolution_clock::now();
      uint64_t ns = chrono::duration_cast<chrono::nanoseconds>(end_ts - begin_ts).count();
      reads_per_second = (OPERATION_COUNT * 1e9) / ns;
   }

   //@formatter:off
   cout << "res:"
        << " technique: " << competitor_name
        << " order: " << (SEQUENTIAL ? "seq" : "rand")
        << " entry_size: " << ENTRY_SIZE
        << " updates(M): " << updates_per_second / 1000 / 1000.0
        << " reads(M): " << reads_per_second / 1000 / 1000.0
        << " dep_reads(M): " << dep_reads_per_second / 1000 / 1000.0
        << endl;
   //@formatter:on

   if (VALIDATE) {
      if (expected.size() != operations.size()) {
         cout << "validation failed (size) !!" << endl;
         throw;
      }
      for (uint64_t i = 0; i<expected.size(); i++) {
         if (expected[i] != operations[i]) {
            cout << "validation failed !! " << i << endl;
            throw;
         }
      }
      cout << "ok" << endl;
   }
}
// -------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
   if (argc != 5) {
      cout << "usage: " << argv[0] << " operation_count entry_count [seq|rand] nvm_path" << endl;
      throw;
   }

   if (ENTRY_SIZE % 4 != 0) {
      cout << "ENTRY_SIZE needs to be a multiple of 4 for now" << endl;
      throw;
   }

   // Config
   ENTRY_COUNT = atof(argv[1]);
   OPERATION_COUNT = atof(argv[2]);
   SEQUENTIAL = argv[3][0] == 's';
   NVM_PATH = argv[4]; // Path to the nvm folder

   cout << "Config" << endl;
   cout << "------" << endl;
   cout << "entry_size         " << ENTRY_SIZE << endl;
   cout << "entry_count(M)     " << ENTRY_COUNT / 1000 / 1000.0 << endl;
   cout << "needed_data(GB)    " << ENTRY_COUNT * ENTRY_SIZE / 1000 / 1000 / 1000.0 << endl;
   cout << "actual_data(GB)    " << ENTRY_COUNT * sizeof(Operation<ENTRY_SIZE>) / 1000 / 1000 / 1000.0 << endl;
   cout << "operation_count(M) " << OPERATION_COUNT / 1000 / 1000.0 << endl;
   cout << "order              " << (SEQUENTIAL ? "sequential" : "random") << endl;
   cout << "nvm_path           " << NVM_PATH << endl;
   cout << "------" << endl;

   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   CPU_SET(0, &cpuset);
   pthread_t currentThread = pthread_self();
   if (pthread_setaffinity_np(currentThread, sizeof(cpu_set_t), &cpuset) != 0) {
      throw;
   }

   // Sequential Experiments
   if (SEQUENTIAL) {
      auto seq_operation_vec = PrepareSeqentialOperations();
      RunExperiment<LogBasedUpdates<ENTRY_SIZE>>("log", seq_operation_vec);
      RunExperiment<cow::CowBasedUpdates<ENTRY_SIZE>>("cow", seq_operation_vec);
      RunExperiment<high::InPlaceLikeUpdates<ENTRY_SIZE>>("high-bit", seq_operation_vec);
      RunExperiment<sliding::InPlaceLikeUpdates<ENTRY_SIZE>>("sliding-bit", seq_operation_vec);
   }

   // Random
   if (!SEQUENTIAL) {
      auto rand_operation_vec = PrepareRandomOperations();
      RunExperiment<LogBasedUpdates<ENTRY_SIZE>>("log", rand_operation_vec);
      RunExperiment<cow::CowBasedUpdates<ENTRY_SIZE>>("cow", rand_operation_vec);
      RunExperiment<high::InPlaceLikeUpdates<ENTRY_SIZE>>("high-bit", rand_operation_vec);
      RunExperiment<sliding::InPlaceLikeUpdates<ENTRY_SIZE>>("sliding-bit", rand_operation_vec);
   }

   cout << "done 3" << endl;
   return 0;
}
// -------------------------------------------------------------------------------------

#include "InPlace-highBit.hpp"
#include "InPlace-sliding.hpp"
#include "LogBased.hpp"
#include "CowBased.hpp"
#include "ValidationBased.hpp"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <chrono>
// -------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------
#ifndef ENTRY_SIZE
#error Please define ENTRY_SIZE
#define ENTRY_SIZE 16
#endif
// -------------------------------------------------------------------------------------
uint64_t ENTRY_COUNT;
uint64_t DATA_SIZE;
string NVM_PATH;
vector<Operation<ENTRY_SIZE>> log_result;
constexpr bool VALIDATE = false;
bool SEQUENTIAL;
// -------------------------------------------------------------------------------------
vector<Operation<ENTRY_SIZE>> PrepareSequentialOperations()
{
   std::vector<Operation<ENTRY_SIZE>> results;
   results.resize(ENTRY_COUNT);
   for (uint64_t i = 0; i<ENTRY_COUNT; i++) {
      results[i].entry_id = (i + 1) % ENTRY_COUNT; // Need +1 to be able to read from i and then go to i+1
      memset(results[i].data.data(), (char) i, results[i].data.size());
   }
   return results;
}
// -------------------------------------------------------------------------------------
vector<Operation<ENTRY_SIZE>> PrepareRandomOperations()
{
   // Init
   uint64_t *helper = new uint64_t[ENTRY_COUNT];
   for (uint64_t i = 0; i<ENTRY_COUNT; i++) {
      helper[i] = i;
   }

   // Shuffle
   Random ranny;
   for (uint64_t i = 0; i<ENTRY_COUNT; i++) {
      swap(helper[i], helper[ranny.Rand() % ENTRY_COUNT]);
   }

   // Assign
   std::vector<Operation<ENTRY_SIZE>> results;
   results.resize(ENTRY_COUNT);
   for (uint64_t i = 0; i<ENTRY_COUNT; i++) {
      results[helper[i]].entry_id = helper[(i + 1) % ENTRY_COUNT];
      memset(results[i].data.data(), (char) i, results[i].data.size());
   }
   delete[] helper;

   return results;
}
// -------------------------------------------------------------------------------------
template<class COMPETITOR>
void RunExperiment(const vector<Operation<ENTRY_SIZE>> &operations, const std::string &competitor_name)
{
   Operation<ENTRY_SIZE> buffer = {};
   vector<uint32_t> ids_only;
   ids_only.reserve(operations.size());
   for (auto &iter : operations) {
      ids_only.push_back(iter.entry_id);
   }

   COMPETITOR competitor(NVM_PATH, ENTRY_COUNT);

   uint64_t check_sum_to_prevent_optimizations = 0;

   uint64_t updates_per_second = 0;
   uint64_t reads_per_second = 0;
   uint64_t dep_reads_per_second = 0;

   // Updates
   {
      auto begin_ts = chrono::high_resolution_clock::now();
      for (uint64_t u = 0; u<operations.size(); u++) {
         competitor.DoUpdate(operations[u], operations[u].entry_id);
      }
      auto end_ts = chrono::high_resolution_clock::now();
      uint64_t ns = chrono::duration_cast<chrono::nanoseconds>(end_ts - begin_ts).count();
      updates_per_second = (operations.size() * 1e9) / ns;
   }

   // Reads
   {
      auto begin_ts = chrono::high_resolution_clock::now();
      for (uint64_t u = 0; u<operations.size(); u++) {
         check_sum_to_prevent_optimizations += competitor.ReadSingleResult(buffer, ids_only[u]);
      }
      auto end_ts = chrono::high_resolution_clock::now();
      uint64_t ns = chrono::duration_cast<chrono::nanoseconds>(end_ts - begin_ts).count();
      reads_per_second = (operations.size() * 1e9) / ns;
   }

   // Dependent reads
   {
      for (uint64_t u = 0; u<operations.size(); u++) {
         competitor.DoUpdate(operations[u], u);
      }

      auto begin_ts = chrono::high_resolution_clock::now();
      uint64_t next_id = 0;
      for (uint64_t u = 0; u<operations.size(); u++) {
         next_id = competitor.ReadSingleResult(buffer, next_id);
      }
      check_sum_to_prevent_optimizations += next_id;
      auto end_ts = chrono::high_resolution_clock::now();
      uint64_t ns = chrono::duration_cast<chrono::nanoseconds>(end_ts - begin_ts).count();
      dep_reads_per_second = (operations.size() * 1e9) / ns;
   }

   //@formatter:off
   cout << "res:"
        << " technique: " << competitor_name
        << " checksum: " << check_sum_to_prevent_optimizations
        << " order: " << (SEQUENTIAL ? "seq" : "rand")
        << " entry_size: " << ENTRY_SIZE
        << " updates(M): " << updates_per_second / 1000 / 1000.0
        << " reads(M): " << reads_per_second / 1000 / 1000.0
        << " dep_reads(M): " << dep_reads_per_second / 1000 / 1000.0
        << endl;
   //@formatter:on

   if constexpr (VALIDATE) {
      Operation<ENTRY_SIZE> validation_buffer;
      ValidationBased<ENTRY_SIZE> validation(NVM_PATH, ENTRY_COUNT);
      for (uint64_t u = 0; u<ENTRY_COUNT; u++) {
         validation.DoUpdate(operations[u], u);
      }

      for (ub4 i = 0; i<operations.size(); i++) {
         validation.ReadSingleResult(validation_buffer, operations[i].entry_id);
         competitor.ReadSingleResult(buffer, operations[i].entry_id);
         if (validation_buffer != buffer) {
            cerr << "validation failed! at i=" << i << endl;
            throw;
         }
      }
      cout << "validation ok ! *hurray* :)" << endl;
   }
}
// -------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
   if (argc != 4) {
      cout << "usage: " << argv[0] << " data_size [seq|rand] nvm_path" << endl;
      throw;
   }

   if (ENTRY_SIZE % 4 != 0) {
      cout << "ENTRY_SIZE needs to be a multiple of 4 for now" << endl;
      throw;
   }

   // Config
   DATA_SIZE = atof(argv[1]);
   SEQUENTIAL = argv[2][0] == 's';
   NVM_PATH = argv[3]; // Path to the nvm folder
   ENTRY_COUNT = DATA_SIZE / ENTRY_SIZE;

   cout << "Config" << endl;
   cout << "------" << endl;
   cout << "entry_size         " << ENTRY_SIZE << endl;
   cout << "data_size          " << DATA_SIZE << endl;
   cout << "entry_count(M)     " << ENTRY_COUNT / 1000 / 1000.0 << endl;
   cout << "needed_data(GB)    " << ENTRY_COUNT * ENTRY_SIZE / 1000 / 1000 / 1000.0 << endl;
   cout << "actual_data(GB)    " << ENTRY_COUNT * sizeof(Operation<ENTRY_SIZE>) / 1000 / 1000 / 1000.0 << endl;
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

   if (ENTRY_COUNT == 0) {
      cout << "need at least one entry" << endl;
      throw;
   }

   // Sequential Experiments
   if (SEQUENTIAL) {
      vector<Operation<ENTRY_SIZE>> operations = PrepareSequentialOperations();
      RunExperiment<LogBasedUpdates<ENTRY_SIZE>>(operations, "log");
      RunExperiment<cow::CowBasedUpdates<ENTRY_SIZE>>(operations, "cow");
      //      RunExperiment<high::InPlaceLikeUpdates<ENTRY_SIZE>>(operations, "high-bit");
      RunExperiment<sliding::InPlaceLikeUpdates<ENTRY_SIZE>>(operations, "sliding-bit");
   }

   // Random
   if (!SEQUENTIAL) {
      vector<Operation<ENTRY_SIZE>> operations = PrepareRandomOperations();
      RunExperiment<LogBasedUpdates<ENTRY_SIZE>>(operations, "log");
      RunExperiment<cow::CowBasedUpdates<ENTRY_SIZE>>(operations, "cow");
      //      RunExperiment<high::InPlaceLikeUpdates<ENTRY_SIZE>>(operations, "high-bit");
      RunExperiment<sliding::InPlaceLikeUpdates<ENTRY_SIZE>>(operations, "sliding-bit");
   }

   cout << "done 3" << endl;
   return 0;
}
// -------------------------------------------------------------------------------------

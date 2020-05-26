#include "InPlace-highBit.hpp"
#include "InPlace-sliding.hpp"
#include "LogBased.hpp"
#include "CowBased.hpp"
#include "ValidationBased.hpp"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <chrono>
#include <algorithm>
#include <fstream>
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
const uint64_t INDIVIDUAL_OPERATION_COUNT = 10000;
const uint64_t ITERATION_COUNT = 10;
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
tuple<double, double, double> GetConfidenceIntervall(vector<uint64_t> &individual_times, string hint)
{
   sort(individual_times.begin(), individual_times.end());

   double lower_time = individual_times[INDIVIDUAL_OPERATION_COUNT * 0.025];
   double med_time = individual_times[INDIVIDUAL_OPERATION_COUNT * 0.50];
   double upper_time = individual_times[INDIVIDUAL_OPERATION_COUNT * 0.975];
   double lower_per_second = 1e9 / upper_time; // swapped!!
   double med_per_second = 1e9 / med_time;
   double upper_per_second = 1e9 / lower_time;

   return make_tuple(med_per_second, med_per_second - lower_per_second, upper_per_second - med_per_second);
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

   // Updates throughput -> execute many updates and take the avg time
   vector<double> updates_per_second;
   for (uint32_t iteration = 0; iteration<ITERATION_COUNT; iteration++) {
      auto begin_ts = chrono::high_resolution_clock::now();
      for (uint64_t u = 0; u<operations.size(); u++) {
         competitor.DoUpdate(operations[u], operations[u].entry_id);
      }
      auto end_ts = chrono::high_resolution_clock::now();
      uint64_t ns = chrono::duration_cast<chrono::nanoseconds>(end_ts - begin_ts).count();
      updates_per_second.push_back((operations.size() * 1e9) / ns);
   }

   //   // Individual updates -> get the time for individual updates
   //   double lower_updates_per_second_factor = 0;
   //   double upper_updates_per_second_factor = 0;
   //   double med_updates_per_second = 0;
   //   {
   //      vector<uint64_t> times(INDIVIDUAL_OPERATION_COUNT);
   //      for (uint64_t i = 0; i<INDIVIDUAL_OPERATION_COUNT; i++) {
   //         auto begin_ts = chrono::high_resolution_clock::now();
   //         competitor.DoUpdate(operations[i], operations[i].entry_id);
   //         auto end_ts = chrono::high_resolution_clock::now();
   //         times[i] = chrono::duration_cast<chrono::nanoseconds>(end_ts - begin_ts).count();
   //         alex_MFence();
   //      }
   //      tie(med_updates_per_second, lower_updates_per_second_factor, upper_updates_per_second_factor) = GetConfidenceIntervall(times, "update");
   //   }

   // Read throughput -> execute many reads and take the avg time
   vector<double> reads_per_second;
   for (uint32_t iteration = 0; iteration<ITERATION_COUNT; iteration++) {
      auto begin_ts = chrono::high_resolution_clock::now();
      for (uint64_t u = 0; u<operations.size(); u++) {
         check_sum_to_prevent_optimizations += competitor.ReadSingleResult(buffer, ids_only[u]);
      }
      auto end_ts = chrono::high_resolution_clock::now();
      uint64_t ns = chrono::duration_cast<chrono::nanoseconds>(end_ts - begin_ts).count();
      reads_per_second.push_back((operations.size() * 1e9) / ns);
   }

   //   // Individual reads -> get the time for individual reads
   //   double lower_reads_per_second_factor = 0;
   //   double upper_reads_per_second_factor = 0;
   //   double med_reads_per_second = 0;
   //   {
   //      vector<uint64_t> times(INDIVIDUAL_OPERATION_COUNT);
   //      for (uint64_t i = 0; i<INDIVIDUAL_OPERATION_COUNT; i++) {
   //         auto begin_ts = chrono::high_resolution_clock::now();
   //         check_sum_to_prevent_optimizations += competitor.ReadSingleResult(buffer, ids_only[i]);
   //         auto end_ts = chrono::high_resolution_clock::now();
   //         times[i] = chrono::duration_cast<chrono::nanoseconds>(end_ts - begin_ts).count();
   //         alex_MFence();
   //      }
   //      tie(med_reads_per_second, lower_reads_per_second_factor, upper_reads_per_second_factor) = GetConfidenceIntervall(times, "reads");
   //   }

   // Dependent read throughput -> execute many dependent reads and take the avg time
   vector<double> dep_reads_per_second;
   for (uint32_t iteration = 0; iteration<ITERATION_COUNT; iteration++) {
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
      dep_reads_per_second.push_back((operations.size() * 1e9) / ns);
   }

   //   // Individual dependent reads -> get the time for individual dependent reads
   //   double lower_dep_reads_per_second_factor = 0;
   //   double upper_dep_reads_per_second_factor = 0;
   //   double med_dep_reads_per_second = 0;
   //   {
   //      for (uint64_t u = 0; u<operations.size(); u++) {
   //         competitor.DoUpdate(operations[u], u);
   //      }
   //
   //      vector<uint64_t> times(INDIVIDUAL_OPERATION_COUNT);
   //      uint64_t next_id = 0;
   //      for (uint64_t i = 0; i<INDIVIDUAL_OPERATION_COUNT; i++) {
   //         auto begin_ts = chrono::high_resolution_clock::now();
   //         next_id = competitor.ReadSingleResult(buffer, next_id);
   //         auto end_ts = chrono::high_resolution_clock::now();
   //         times[i] = chrono::duration_cast<chrono::nanoseconds>(end_ts - begin_ts).count();
   //         alex_MFence();
   //      }
   //      check_sum_to_prevent_optimizations += next_id;
   //      tie(med_dep_reads_per_second, lower_dep_reads_per_second_factor, upper_dep_reads_per_second_factor) = GetConfidenceIntervall(times, "dep");
   //   }

   //   //@formatter:off
//   cout << "res:"
//        << " technique: " << competitor_name
//        << " checksum: " << check_sum_to_prevent_optimizations
//        << " order: " << (SEQUENTIAL ? "seq" : "rand")
//        << " entry_size: " << ENTRY_SIZE
//        << " updates(M): " << med_updates_per_second / 1000 / 1000.0
//        << " " << (lower_updates_per_second_factor)  / 1000 / 1000.0
//        << " " << (upper_updates_per_second_factor) / 1000 / 1000.0
//        << " reads(M): " << med_reads_per_second / 1000 / 1000.0
//        << " " << (lower_reads_per_second_factor) / 1000 / 1000.0
//        << " " << (upper_reads_per_second_factor) / 1000 / 1000.0
//        << " dep_reads(M): " << med_dep_reads_per_second / 1000 / 1000.0
//        << " " << (lower_dep_reads_per_second_factor)  / 1000 / 1000.0
//        << " " << (upper_dep_reads_per_second_factor) / 1000 / 1000.0
//        << endl;
//   //@formatter:on

   //   //@formatter:off
//   cout << "res:"
//        << " technique: " << competitor_name
//        << " checksum: " << check_sum_to_prevent_optimizations
//        << " order: " << (SEQUENTIAL ? "seq" : "rand")
//        << " entry_size: " << ENTRY_SIZE
//        << " updates(M): " << updates_per_second / 1000 / 1000.0
//        << " " << (updates_per_second - updates_per_second * lower_updates_per_second_factor)  / 1000 / 1000.0
//        << " " << (updates_per_second * upper_updates_per_second_factor - updates_per_second) / 1000 / 1000.0
//        << " reads(M): " << reads_per_second / 1000 / 1000.0
//        << " " << (reads_per_second - reads_per_second * lower_reads_per_second_factor) / 1000 / 1000.0
//        << " " << (reads_per_second * upper_reads_per_second_factor - reads_per_second) / 1000 / 1000.0
//        << " dep_reads(M): " << dep_reads_per_second / 1000 / 1000.0
//        << " " << (dep_reads_per_second - dep_reads_per_second * lower_dep_reads_per_second_factor)  / 1000 / 1000.0
//        << " " << (dep_reads_per_second * upper_dep_reads_per_second_factor - dep_reads_per_second) / 1000 / 1000.0
//        << endl;
//   //@formatter:on

   sort(updates_per_second.begin(), updates_per_second.end());
   sort(reads_per_second.begin(), reads_per_second.end());
   sort(dep_reads_per_second.begin(), dep_reads_per_second.end());

   //@formatter:off
   cout << "res:"
        << " technique: " << competitor_name
        << " checksum: " << check_sum_to_prevent_optimizations
        << " order: " << (SEQUENTIAL ? "seq" : "rand")
        << " entry_size: " << ENTRY_SIZE
        << " updates(M): " << (updates_per_second[4] + updates_per_second[5]) / 2e6
        << " " << updates_per_second[0]  / 1e6
        << " " << updates_per_second[9] / 1e6
        << " reads(M): " << (reads_per_second[4] + reads_per_second[5]) / 2e6
        << " " << reads_per_second[0] / 1e6
        << " " << reads_per_second[9] / 1e6
        << " dep_reads(M): " << (dep_reads_per_second[4] + dep_reads_per_second[5]) / 2e6
        << " " << dep_reads_per_second[0]  / 1e6
        << " " << dep_reads_per_second[9] / 1e6
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
   if (ENTRY_COUNT<INDIVIDUAL_OPERATION_COUNT) {
      cout << "Need higher ENTRY_COUNT than INDIVIDUAL_OPERATION_COUNT" << endl;
      throw;
   }

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

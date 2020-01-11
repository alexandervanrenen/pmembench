#include "InPlace1.hpp"
#include "InPlace2.hpp"
#include "InPlace2SIMD.hpp"
#include "InPlace3.hpp"
#include "InPlace3SIMD.hpp"
#include "LogBased.hpp"
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
uint64_t DATA_SIZE;
uint64_t ENTRY_COUNT;
string NVM_PATH;
const uint64_t UNIQUE_STRING_COUNT = 128;
// -------------------------------------------------------------------------------------
template<class COMPETITOR>
void RunExperiment(const string &technique_name, COMPETITOR &competitor)
{
   auto update_vec = competitor.PrepareUpdates(UNIQUE_STRING_COUNT);
   Random ranny;

   auto begin_ts = chrono::high_resolution_clock::now();

   for (uint64_t u = 0; u<UPDATE_COUNT; u++) {
      competitor.DoUpdate(ranny.Rand() % ENTRY_COUNT, update_vec[u % UNIQUE_STRING_COUNT]);
   }

   auto end_ts = chrono::high_resolution_clock::now();
   uint64_t ns = chrono::duration_cast<chrono::nanoseconds>(end_ts - begin_ts).count();
   uint64_t updates_per_second = (UPDATE_COUNT * 1e9) / ns;

   //@formatter:off
   cout << "res:"
        << " technique: " << technique_name
        << " entry_size: " << ENTRY_SIZE
        << " updates_per_second: " << updates_per_second
        << endl;
   //@formatter:on
}
// -------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
   if (argc != 4) {
      cout << "usage: " << argv[0] << " update_count data_size nvm_path" << endl;
      throw;
   }

   if (ENTRY_SIZE % 4 != 0) {
      cout << "ENTRY_SIZE needs to be a multiple of 4 for now" << endl;
      throw;
   }

   // Config
   UPDATE_COUNT = atof(argv[1]);
   DATA_SIZE = atof(argv[2]);
   NVM_PATH = argv[3]; // Path to the nvm folder
   ENTRY_COUNT = DATA_SIZE / ENTRY_SIZE;

   cout << "Config" << endl;
   cout << "------" << endl;
   cout << "entry_size         " << ENTRY_SIZE << endl;
   cout << "entry_count        " << ENTRY_COUNT << endl;
   cout << "data_size          " << DATA_SIZE << endl;
   cout << "update_count       " << UPDATE_COUNT << endl;
   cout << "nvm_path           " << NVM_PATH << endl;
   cout << "------" << endl;

   const bool VALIDATE = false;

   // Testing / Validation code
   //   TestSomeInPlaceUpdateConfigurations();

   // Run Experiments
   LogBasedUpdates<ENTRY_SIZE> log_based(NVM_PATH, ENTRY_COUNT, 10e9);
   RunExperiment("log", log_based);
   char *log_result = log_based.GetResult();

   if (v1::InPlaceLikeUpdates<ENTRY_SIZE>::CanBeUsed(ENTRY_SIZE)) {
      v1::InPlaceLikeUpdates<ENTRY_SIZE> tech(NVM_PATH, ENTRY_COUNT);
      RunExperiment("generic-loop", tech);

      if (VALIDATE) {
         char *res = tech.CreateResult();
         cout << "ok: " << memcmp(res, log_result, ENTRY_COUNT * ENTRY_SIZE) << endl;
      }
   }

   if (v2::InPlaceLikeUpdates<ENTRY_SIZE>::CanBeUsed(ENTRY_SIZE)) {
      v2::InPlaceLikeUpdates<ENTRY_SIZE> tech(NVM_PATH, ENTRY_COUNT);
      RunExperiment("high-bits", tech);

      if (VALIDATE) {
         char *res = tech.CreateResult();
         cout << "ok: " << memcmp(res, log_result, ENTRY_COUNT * ENTRY_SIZE) << endl;
      }
   }

   if (v2simd::InPlaceLikeUpdates<ENTRY_SIZE>::CanBeUsed(ENTRY_SIZE)) {
      v2simd::InPlaceLikeUpdates<ENTRY_SIZE> tech(NVM_PATH, ENTRY_COUNT);
      RunExperiment("high-bits-simd(stef)", tech);

      if (VALIDATE) {
         char *res = tech.CreateResult();
         cout << "ok: " << memcmp(res, log_result, ENTRY_COUNT * ENTRY_SIZE) << endl;
      }
   }

   if (v3::InPlaceLikeUpdates<ENTRY_SIZE>::CanBeUsed(ENTRY_SIZE)) {
      v3::InPlaceLikeUpdates<ENTRY_SIZE> tech(NVM_PATH, ENTRY_COUNT);
      RunExperiment("moving-version", tech);

      if (VALIDATE) {
         char *res = tech.CreateResult();
         cout << "ok: " << memcmp(res, log_result, ENTRY_COUNT * ENTRY_SIZE) << endl;
      }
   }

   if (v3simd::InPlaceLikeUpdates<ENTRY_SIZE>::CanBeUsed(ENTRY_SIZE)) {
      v3simd::InPlaceLikeUpdates<ENTRY_SIZE> tech(NVM_PATH, ENTRY_COUNT);
      RunExperiment("moving-version-simd", tech);

      if (VALIDATE) {
         char *res = tech.CreateResult();
         cout << "ok: " << memcmp(res, log_result, ENTRY_COUNT * ENTRY_SIZE) << endl;
      }
   }

   //   {
   //      CowBasedUpdates<ENTRY_SIZE> cow_based(NVM_PATH);
   //      RunExperiment("cow", log_based, strings);
   //   }

   return 0;
}
// -------------------------------------------------------------------------------------

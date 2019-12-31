#include "FullBufferFrame.hpp"
#include "Pages.hpp"
#include "Random.hpp"
#include "VolatileMemory.hpp"
#include "NonVolatileMemory.hpp"
#include <set>
#include <thread>
#include <atomic>
#include <inttypes.h>
#include <unordered_map>
#include <libpmemblk.h>
#include <functional>

using namespace std;

// Config
ub4 PAGE_COUNT; // How many pages to use
ub4 DIRTY_CL_COUNT; // How many cache lines are dirty
string NVM_FILE; // Path to the nvm file
ub4 THREAD_COUNT; // How many threads -> 0 runs single threaded config
ub4 RUN_COUNT; // How often should the workload be run (only for MT)
ub4 PAGE_COUNT_PER_THREAD;

ub8 RunWithTiming(function<void()> foo)
{
   auto begin = chrono::high_resolution_clock::now();
   foo();
   auto end = chrono::high_resolution_clock::now();
   return chrono::duration_cast<chrono::nanoseconds>(end - begin).count();
}

struct FlushTest {

   Random ranny;

   ub8 page_count;

   VolatileMemory ram;
   NonVolatileMemory nvm; // One more page than in dram, because of shadow
   unordered_map<ub4, NvmBufferFrame *> nvm_mapping;

   NvmBufferFrame *free_nvm_bf;
   MicroLog *micro_log;
   MicroLog2 *micro_log_2;

   NvmBufferFrame *GetMappedNvmBf(ub8 id) { return nvm_mapping.find(id)->second; }
   FullBufferFrame *GetRamBf(ub8 id) { return ram.GetPtr<FullBufferFrame>(id); }

   PMEMblkpool *pbp;

   FlushTest(const string &nvm_file, ub8 page_count)
           : page_count(page_count)
             , ram(page_count * sizeof(FullBufferFrame))
             , nvm(nvm_file, (page_count + 10) * sizeof(NvmBufferFrame)) // + 10 to have some space after the memory for the pages to put the micro log or the shadow-copy page of CoW
   {
      auto file = (nvm_file + "_pmemlib");

      // Setup pmdk blk pool
      ub8 pmemlib_pool_size = constants::kPageByteCount * page_count + PMEMBLK_MIN_POOL; // A bit to much, but ensures that it works..
      system(("rm -rf " + file).c_str());
      pbp = pmemblk_create(file.c_str(), constants::kPageByteCount, pmemlib_pool_size, 0666);
      if (pbp == NULL) {
         cout << "failed creating pmdk pmem pool" << endl;
         assert(false);
         throw;
      }

      // Check that we got enough pages in the pool (should always work)
      ub4 nelements = pmemblk_nblock(pbp);
      if (nelements<page_count) {
         cout << "pmdk pool is too small" << endl;
         assert(false);
         throw;
      }
   }
   FlushTest(const FlushTest &) = delete;
   FlushTest &operator=(const FlushTest &) = delete;

   // Set nvm and dram content to 'x'
   void InitializePages()
   {
      memset(ram.Data(), 'x', ram.GetByteCount());
      memset(nvm.Data(), 'x', nvm.GetByteCount());
      for (ub4 p = 0; p<page_count; ++p) {
         NvmBufferFrame *nvm_bf = &nvm.GetNvmBufferFrame(p);
         nvm_bf->init();
         nvm_bf->page_id = p;
         GetRamBf(p)->Init();
         GetRamBf(p)->SwapIn(p, nvm_bf);
         nvm_mapping[p] = nvm_bf;
      }

      // All techniques (cow and micro log) need a small buffer, we put this after all regular pages
      // The buffers also overlap because only one of them is used in a given experiment
      // Kind of a hacky design, but good enough for some benchmark code
      free_nvm_bf = &nvm.GetNvmBufferFrame(page_count);

      // Logs need to be aligned nicely for SIMD cpy
      ub1 *nice_aligned_position = reinterpret_cast<ub1 *>(&nvm.GetNvmBufferFrame(page_count));
      while (ub8(nice_aligned_position) % 64 != 0) {
         nice_aligned_position++;
      }
      micro_log = reinterpret_cast<MicroLog *>(nice_aligned_position);
      micro_log_2 = reinterpret_cast<MicroLog2 *>(nice_aligned_position);
      assert(reinterpret_cast<ub1 *>(micro_log) + sizeof(MicroLog)<nvm.Data() + nvm.GetByteCount());
      assert(reinterpret_cast<ub1 *>(micro_log_2) + sizeof(MicroLog2)<nvm.Data() + nvm.GetByteCount());
   }

   // Set all DRAM content to 'a'
   void MakeAllCacheLinesDirty()
   {
      for (ub4 i = 0; i<page_count; ++i) {
         ub1 *data = GetRamBf(i)->Translate<EnsureMode::MarkDirty>();
         memset(data, 'a', constants::kPageByteCount);
      }
   }

   // Set cl_count cls to 'a' in DRAM and those which are still 'x' in dram to 'a' in NVM.
   // Hence, 'a' is the dirty data in DRAM and after a flush everything on NVM should be 'a'.
   void MakeRandomCacheLinesDirty(ub4 cl_count, bool make_other_cls_resident)
   {
      set<ub4> offsets;
      for (ub4 i = 0; i<constants::kCacheLinesPerPage; ++i) {
         offsets.insert(i);
      }

      for (ub4 p = 0; p<page_count; ++p) {
         set<ub4> offsets_cpy = offsets;
         for (ub4 i = 0; i<cl_count; ++i) {
            assert(!offsets_cpy.empty());
            auto random_cl = offsets_cpy.begin();
            advance(random_cl, ranny.Rand() % offsets_cpy.size());
            ub4 cl = *random_cl;
            ub1 *data = GetRamBf(p)->Translate<EnsureMode::MarkDirty>(cl * constants::kCacheLineByteCount, constants::kCacheLineByteCount);
            memset(data, 'a', constants::kCacheLineByteCount);
            offsets_cpy.erase(random_cl);
         }

         for (auto iter : offsets_cpy) {
            ub1 *data = GetMappedNvmBf(p)->GetPage().Ptr() + iter * constants::kCacheLineByteCount;
            memset(data, 'a', constants::kCacheLineByteCount);
            if (make_other_cls_resident) {
               ub1 *ram_data = GetRamBf(p)->Translate<EnsureMode::Ensure>(iter * constants::kCacheLineByteCount, constants::kCacheLineByteCount);
               memset(ram_data, 'a', constants::kCacheLineByteCount);
            }
         }

         // Check
         if (make_other_cls_resident) {
            assert(GetRamBf(p)->GetResidentCacheLineCount() == constants::kCacheLinesPerPage);
         } else {
            assert(GetRamBf(p)->GetResidentCacheLineCount() == cl_count);
         }
         assert(GetRamBf(p)->GetDirtyCacheLineCount() == cl_count);
      }
   }

   void FlushAll_Strawman()
   {
      for (ub4 i = 0; i<page_count; ++i) {
         GetRamBf(i)->Flush();
      }
   }

   void FlushAll_PMDK()
   {
      for (ub4 p = 0; p<page_count; ++p) {
         if (GetRamBf(p)->IsAnythingDirty()) {
            pmemblk_write(pbp, GetRamBf(p)->RamPtr<ub1>(), p);
            GetRamBf(p)->MakeCleanAfterFlushOfPMDK();
         }
      }
   }

   void FlushAll_Shadow()
   {
      ub8 lsn = 0;
      free_nvm_bf->init();
      free_nvm_bf->page_id = constants::kInvalidPageId;

      for (ub4 p = 0; p<page_count; ++p) {
         if (GetRamBf(p)->IsAnythingDirty()) {
            NvmBufferFrame *new_free_one = GetRamBf(p)->FlushShadow(free_nvm_bf, lsn++);
            assert(GetRamBf(p)->GetNvmBufferFrame() == free_nvm_bf);
            free_nvm_bf = new_free_one;
            nvm_mapping[p] = GetRamBf(p)->GetNvmBufferFrame();
         }
      }
   }

   void FlushAll_MicroLog()
   {
      micro_log->page_id = constants::kInvalidPageId;
      micro_log->count = 0;

      for (ub4 p = 0; p<page_count; ++p) {
         if (GetRamBf(p)->IsAnythingDirty()) {
            GetRamBf(p)->FlushMicroLog(*micro_log);
         }
      }
   }

   void FlushAll_MicroLog2()
   {
      memset(micro_log_2, 0, sizeof(MicroLog2));

      for (ub4 p = 0; p<page_count; ++p) {
         if (GetRamBf(p)->IsAnythingDirty()) {
            GetRamBf(p)->FlushMicroLog(*micro_log_2);
         }
      }
   }

   void PrintPages()
   {
      for (ub4 p = 0; p<page_count; ++p) {
         PrintPage(p);
      }
   }

   void PrintPage(ub4 p)
   {
      cout << p << " ram: ";
      for (ub4 i = 0; i<constants::kPageByteCount; i += 64) {
         cout << GetRamBf(p)->RamPtr<ub1>()[i];
      }
      cout << endl << p << " nvm: ";
      for (ub4 i = 0; i<constants::kPageByteCount; i += 64) {
         cout << GetMappedNvmBf(p)->GetPage().Ptr()[i];
      }
      cout << endl;
   }

   // Checks that each cl in NVM is c
   void CheckNvmContentEqualsTo(char c)
   {
      for (ub4 p = 0; p<page_count; ++p) {
         assert(nvm_mapping[p]->page_id == p);
         assert(GetMappedNvmBf(p)->page_id == p);
         for (ub4 i = 0; i<constants::kPageByteCount; ++i) {
            assert(c == GetMappedNvmBf(p)->GetPage().Ptr()[i]);
         }
      }
   }
};

void RunBenchmarkThreaded(string tech, bool all_resident, function<void(FlushTest &ft)> callback)
{
   atomic<ub4> ready_count(0);
   atomic<bool> start_barrier(false);
   vector<unique_ptr<thread>> threads;
   vector<ub8> times(THREAD_COUNT, 0);
   for (ub4 tid = 0; tid<THREAD_COUNT; tid++) {
      threads.push_back(make_unique<thread>([&, tid]() {
         FlushTest ft(NVM_FILE + string("_") + to_string(tid), PAGE_COUNT_PER_THREAD);
         ready_count++;
         while (!start_barrier);
         for (ub4 run = 0; run<RUN_COUNT; run++) {
            ft.InitializePages();
            ft.MakeRandomCacheLinesDirty(DIRTY_CL_COUNT, all_resident);
            times[tid] += RunWithTiming([&]() { callback(ft); });
         }
         ft.CheckNvmContentEqualsTo('a');
      }));
   }
   while (ready_count != THREAD_COUNT);
   start_barrier = true;
   for (ub4 tid = 0; tid<THREAD_COUNT; tid++) {
      threads[tid]->join();
   }

   ub8 time_sum_of_all_threads = 0;
   for (ub4 tid = 0; tid<THREAD_COUNT; tid++) {
      time_sum_of_all_threads += times[tid];
   }
   double avg_time_per_thread = time_sum_of_all_threads * 1.0 / THREAD_COUNT;
   ub8 total_page_count = PAGE_COUNT * RUN_COUNT;
   ub8 byte_count = total_page_count * DIRTY_CL_COUNT * constants::kCacheLineByteCount;
   double page_per_second = (total_page_count * 1000000000) / (avg_time_per_thread);

   //@formatter:off
   cout << "res"
        << " tech= " << tech
        << " dirty_cl_count= " << DIRTY_CL_COUNT
        << " thread_count= " << THREAD_COUNT
        << " page_count= " << PAGE_COUNT
        << " avg_time= " << avg_time_per_thread
        << " perf(GB/s)= " << (byte_count / avg_time_per_thread)
        << " perf(pages/s)= " << page_per_second
        << endl;
   //@formatter:on
}

void RunMultiThreaded()
{
   // PMDK
   RunBenchmarkThreaded("PMDK", true, [](FlushTest &ft) {
      ft.FlushAll_Shadow();
   });

   // Shadow some resident
   RunBenchmarkThreaded("Shadow", false, [](FlushTest &ft) {
      ft.FlushAll_Shadow();
   });

   // Shadow all resident
   RunBenchmarkThreaded("ShadowResident", true, [](FlushTest &ft) {
      ft.FlushAll_Shadow();
   });

   // Micro log
   RunBenchmarkThreaded("Micro", false, [](FlushTest &ft) {
      ft.FlushAll_MicroLog();
   });
}

// clang++ -DSTREAMING=1 page_flush/page_flush.cpp -std=c++17 -Invml/src/include/ nvml/src/nondebug/libpmem.a nvml/src/nondebug/libpmemblk.a -g0 -O3 -march=native -lpthread -lndctl -ldaxctl
int main(int argc, char **argv)
{
   if (argc != 6) {
      cout << "usage: " << argv[0] << " page_count dirty_cl_count thread_count run_count path" << endl;
      throw;
   }

   PAGE_COUNT = atof(argv[1]);
   DIRTY_CL_COUNT = atof(argv[2]);
   THREAD_COUNT = atof(argv[3]);
   RUN_COUNT = atof(argv[4]);
   NVM_FILE = argv[5];
   PAGE_COUNT_PER_THREAD = PAGE_COUNT / THREAD_COUNT;

   if (DIRTY_CL_COUNT == 0 && DIRTY_CL_COUNT>constants::kCacheLinesPerPage) {
      cout << "invalid DIRTY_CL_COUNT " << DIRTY_CL_COUNT << endl;
      exit(-1);
   }

   cerr << "Config:" << endl;
   cerr << "----------------------------" << endl;
   cerr << "PAGE_COUNT:     " << PAGE_COUNT << endl;
   cerr << "DIRTY_CL_COUNT: " << DIRTY_CL_COUNT << endl;
   cerr << "THREAD_COUNT:   " << THREAD_COUNT << endl;
   cerr << "RUN_COUNT:      " << RUN_COUNT << endl;
   cerr << "NVM_FILE:       " << NVM_FILE << endl;
#ifdef STREAMING
   cerr << "STREAMING:      " << "yes" << endl;
#else
   cerr << "STREAMING:      " << "no" << endl;
#endif

   RunMultiThreaded();

   return 0;
};

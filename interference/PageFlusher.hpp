#pragma once
// -------------------------------------------------------------------------------------
#include "Common.hpp"
#include "FullBufferFrame.hpp"
#include <atomic>
#include <thread>
// -------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------
class PageFlusher {
public:
   atmic<bool> run;
   atmic<bool> ready;
   atmic<bool> stop;

   PageFlusher(const string &nvm_file, ub8 page_count)
           : run(false)
             , ready(false)
             , stop(false)
             , page_count(page_count)
             , ram(page_count * sizeof(FullBufferFrame))
             , nvm(nvm_file, (page_count + 10) * sizeof(NvmBufferFrame)) // + 10 to have some space after the memory for the pages to put the micro log or the shadow-copy page of CoW
   {
      Setup();
      ready = true;
      while (!run) {
         this_thread::yield();
      }

      while (!stop) {
         FlushAll();
      }
   }

   PageFlusher(const PageFlusher &) = delete;
   PageFlusher &operator=(const PageFlusher &) = delete;

private:

   Random ranny;

   ub8 page_count;

   VolatileMemory ram;
   NonVolatileMemory nvm; // One more page than in dram, because of shadow
   unordered_map<ub4, NvmBufferFrame *> nvm_mapping;

   NvmBufferFrame *free_nvm_bf;

   NvmBufferFrame *GetMappedNvmBf(ub8 id) { return nvm_mapping.find(id)->second; }
   FullBufferFrame *GetRamBf(ub8 id) { return ram.GetPtr<FullBufferFrame>(id); }

   // Set nvm and dram content to 'x'
   void Setup()
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
   }

   void FlushAll()
   {
      ub8 lsn = 0;
      free_nvm_bf->init();
      free_nvm_bf->page_id = constants::kInvalidPageId;

      for (ub4 p = 0; p<page_count; ++p) {
         GetRamBf(p)->dirty = true;
         NvmBufferFrame *new_free_one = GetRamBf(p)->FlushShadow(free_nvm_bf, lsn++);
         assert(GetRamBf(p)->GetNvmBufferFrame() == free_nvm_bf);
         free_nvm_bf = new_free_one;
         nvm_mapping[p] = GetRamBf(p)->GetNvmBufferFrame();
      }
   }
};
// -------------------------------------------------------------------------------------
}
// -------------------------------------------------------------------------------------

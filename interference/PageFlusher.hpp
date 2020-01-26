#pragma once
// -------------------------------------------------------------------------------------
#include "Common.hpp"
#include "Pages.hpp"
#include "NonVolatileMemory.hpp"
#include <atomic>
#include <thread>
#include <immintrin.h>
#include <array>
#include <bitset>
#include <cstring>
#include <unordered_map>
#include <iostream>
// -------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------
namespace pf_utils {
// -------------------------------------------------------------------------------------
inline void alex_CopyAndFlush512(void *dest, void *src)
{
   assert((ub8) dest % 64 == 0);
   assert((ub8) src % 64 == 0);
#ifdef STREAMING
   __m512i reg = _mm512_load_si512(src);
   _mm512_stream_si512((__m512i *) dest, reg);
#else
   memcpy(dest, src, constants::kCacheLineByteCount);
   alex_WriteBack(dest);
#endif
}
// -------------------------------------------------------------------------------------
}
// -------------------------------------------------------------------------------------
enum struct EnsureMode : ub1 {
   Ensure, // Loads cache lines
   MarkDirty, // Marks cache lines dirty
   EnsureAndDirty, // Marks cache lines dirty and ensures them
   DoNothing, // Just translates the offset to a pointer
};
// -------------------------------------------------------------------------------------
class FullBufferFrame {
public:
   inline FullBufferFrame();

   template<typename T>
   inline T *RamPtr() { return aligned_page.GetPage().Ptr<T>(); }

   template<EnsureMode mode>
   inline ub1 *Translate();

   template<EnsureMode mode>
   inline ub1 *Translate(ub4 offset, ub4 size);

   // Functions to make cache lines resident
   inline void EnsureResidency();
   inline ub1 *EnsureResidency(ub4 offset, ub4 length);
   inline void EnsureResidency(ub1 *start, ub4 length);
   inline bool IsResident(ub4 offset, ub4 length) const;
   inline ub4 GetResidentCacheLineCount() const { return resident.count(); }

   // Functions to make cache lines dirty
   inline void MarkDirty();
   inline ub1 *MarkDirty(ub4 offset, ub4 length);
   inline void MarkDirty(ub1 *start, ub4 length);

   // Functions to check cache lines
   inline bool IsDirty(ub4 offset, ub4 length) const;
   inline bool IsAnythingDirty() const;
   inline bool IsAnythingResident() const;
   inline ub4 GetDirtyCacheLineCount() const { return dirty.count(); }
   inline void MakeCleanAfterFlushOfPMDK()
   {
      all_dirty = false;
      dirty.set(false);
   }

   // Sanity checks for this buffer frame
   inline bool Invariants();

   // Read page id of the page
   NvmBufferFrame *GetNvmBufferFrame() { return nvm_buffer_frame; }
   const NvmBufferFrame *GetNvmBufferFrame() const { return nvm_buffer_frame; }

private:
   void SetNvmBufferFrame(NvmBufferFrame *nvm_bf) { this->nvm_buffer_frame = nvm_bf; }

   // Which page does this frame correspond to (invalid if non)
   ub8 page_id;

   // A fixed page can not be swapped out
   bool fixed; // Handled by BufferManager

   // Indicates which cache lines of this page are loaded and which are dirty
   bool all_resident;
   std::bitset<constants::kCacheLinesPerPage> resident;
   bool all_dirty;
   std::bitset<constants::kCacheLinesPerPage> dirty;

   // This is a reference to the memory in NV-RAM, required for loading
   NvmBufferFrame *nvm_buffer_frame;

   // This is a D-RAM resident copy of the memory in NV-RAM
   AutoAlignedPage<constants::kPageAlignment> aligned_page;

   // State transformation functions
   inline void Init();
   inline void SwapIn(ub8 pageId, NvmBufferFrame *nvmBufferFrame);
   inline void Fix();
   inline void Unfix();
   inline void Flush();
   inline NvmBufferFrame *FlushShadow(NvmBufferFrame *new_bf, ub8 lsn);
   inline void WriteToNvmComplete();
   inline void WriteToNvmPartial();
   inline void SwapOut();

   friend class PageFlusher;
};
// -------------------------------------------------------------------------------------
FullBufferFrame::FullBufferFrame()
{
}
// -------------------------------------------------------------------------------------
void FullBufferFrame::EnsureResidency()
{
   if (all_resident) {
      return;
   }

   // Fast path: just load everything
   if (resident.none()) {
      memcpy(aligned_page.GetPage().Ptr(), nvm_buffer_frame->GetPage().Ptr(), constants::kPageByteCount);
      all_resident = true;
      return;
   }

   // Otherwise: partial load
   EnsureResidency(ub4(0), constants::kPageByteCount);
   all_resident = true;
}
// -------------------------------------------------------------------------------------
ub1 *FullBufferFrame::EnsureResidency(ub4 offset, ub4 length)
{
   assert(offset<constants::kPageByteCount && offset + length<=constants::kPageByteCount);

   // Get range of cache lines .. yeah some math ;p
   ub4 firstCacheLineId = offset / constants::kCacheLineByteCount;
   ub4 lastCacheLineId = (offset + length - 1) / constants::kCacheLineByteCount + 1;

   // All resident -> just return
   assert(length != 0);
   if (all_resident) {
      return RamPtr<ub1>() + offset;
   }

   // Load each of these cache lines, if they are not already present
   for (ub4 cacheLineId = firstCacheLineId; cacheLineId<lastCacheLineId; ++cacheLineId) {
      if (!resident.test(cacheLineId)) {
         memcpy(aligned_page.GetPage().Ptr() + (cacheLineId * constants::kCacheLineByteCount), nvm_buffer_frame->GetPage().Ptr() + (cacheLineId * constants::kCacheLineByteCount), constants::kCacheLineByteCount);
      }
      resident.set(cacheLineId, true);
   }

   return RamPtr<ub1>() + offset;
}
// -------------------------------------------------------------------------------------
void FullBufferFrame::EnsureResidency(ub1 *start, ub4 length)
{
   assert(RamPtr<ub1>()<=start);
   assert(RamPtr<ub1>() + constants::kPageByteCount>start);
   assert(RamPtr<ub1>() + constants::kPageByteCount>=start + length);
   EnsureResidency(start - RamPtr<ub1>(), length);
}
// -------------------------------------------------------------------------------------
bool FullBufferFrame::IsResident(ub4 offset, ub4 length) const
{
   assert(length != 0);
   assert(offset<constants::kPageByteCount && offset + length<=constants::kPageByteCount);

   if (all_resident) {
      return true;
   }

   // Get range of cache lines .. yeah some math ;p
   ub4 firstCacheLineId = offset / constants::kCacheLineByteCount;
   ub4 lastCacheLineId = (offset + length - 1) / constants::kCacheLineByteCount + 1;

   // Check if each one is resident
   for (ub4 cacheLineId = firstCacheLineId; cacheLineId<lastCacheLineId; ++cacheLineId) {
      if (!resident.test(cacheLineId)) {
         return false;
      }
   }
   return true;
}
// -------------------------------------------------------------------------------------
void FullBufferFrame::MarkDirty()
{

   all_dirty = true;
   all_resident = true;
}
// -------------------------------------------------------------------------------------
ub1 *FullBufferFrame::MarkDirty(ub4 offset, ub4 length)
{
   assert(length != 0);
   assert(offset<constants::kPageByteCount && offset + length<=constants::kPageByteCount);

   // Get range of cache lines .. yeah some math ;p
   ub4 firstCacheLineId = offset / constants::kCacheLineByteCount;
   ub4 lastCacheLineId = (offset + length - 1) / constants::kCacheLineByteCount + 1;

   // Non-nvm backed pages are stored completely
   if (nvm_buffer_frame == nullptr) {
      all_resident = true;
      all_dirty = true;
      return RamPtr<ub1>() + offset;
   }

   // Remember that they are dirty
   for (ub4 cacheLineId = firstCacheLineId; cacheLineId<lastCacheLineId; ++cacheLineId) {
      dirty.set(cacheLineId, true);
      resident.set(cacheLineId, true);
   }

   return RamPtr<ub1>() + offset;
}
// -------------------------------------------------------------------------------------
void FullBufferFrame::MarkDirty(ub1 *start, ub4 length)
{
   assert(RamPtr<ub1>()<=start);
   assert(RamPtr<ub1>() + constants::kPageByteCount>start);
   assert(RamPtr<ub1>() + constants::kPageByteCount>=start + length);
   MarkDirty(start - RamPtr<ub1>(), length);
}
// -------------------------------------------------------------------------------------
bool FullBufferFrame::IsDirty(ub4 offset, ub4 length) const
{
   assert(length != 0);
   assert(offset<constants::kPageByteCount && offset + length<=constants::kPageByteCount);

   if (all_dirty)
      return true;

   // Get range of cache lines .. yeah some math ;p
   ub4 firstCacheLineId = offset / constants::kCacheLineByteCount;
   ub4 lastCacheLineId = (offset + length - 1) / constants::kCacheLineByteCount + 1;

   // Check if each one is resident
   for (ub4 cacheLineId = firstCacheLineId; cacheLineId<lastCacheLineId; ++cacheLineId) {
      if (!dirty.test(cacheLineId)) {
         return false;
      }
   }
   return true;
}
// -------------------------------------------------------------------------------------
bool FullBufferFrame::IsAnythingDirty() const
{
   return all_dirty || dirty.any();
}
// -------------------------------------------------------------------------------------
bool FullBufferFrame::IsAnythingResident() const
{
   return all_resident || resident.any();
}
// -------------------------------------------------------------------------------------
bool FullBufferFrame::Invariants()
{
   if (page_id == constants::kInvalidPageId)
      return true;

   if (all_dirty && !all_resident)
      return false;

   for (ub4 cacheLineId = 0; cacheLineId<constants::kCacheLinesPerPage; ++cacheLineId) {
      if (dirty.test(cacheLineId) && !resident.test(cacheLineId))
         return false;
   }
   return true;
}
// -------------------------------------------------------------------------------------
void FullBufferFrame::Init()
{
   page_id = constants::kInvalidPageId;
   fixed = false;
   all_resident = false;
   resident.reset();
   all_dirty = false;
   dirty.reset();
   nvm_buffer_frame = nullptr;
}
// -------------------------------------------------------------------------------------
void FullBufferFrame::SwapIn(ub8 pageId, NvmBufferFrame *nvmBufferFrame)
{
   assert(resident.count() == 0);
   assert(dirty.count() == 0);
   assert(!all_dirty);
   assert(!all_resident);

   this->page_id = pageId;
   this->nvm_buffer_frame = nvmBufferFrame;
}
// -------------------------------------------------------------------------------------
void FullBufferFrame::Fix()
{
   fixed = true;
}
// -------------------------------------------------------------------------------------
void FullBufferFrame::Unfix()
{
   fixed = false;
}
// -------------------------------------------------------------------------------------
void FullBufferFrame::Flush()
{
   // Otherwise: The page is nvm backed: Everything is dirty -> complete flush to nvm
   if (all_dirty) {
      WriteToNvmComplete();
      return;
   }

   // Otherwise: Do partial flush (necessary, because we would overwrite something)
   if (dirty.count() != 0) {
      WriteToNvmPartial(); // TODO opt .. fall back to complete flush if everything is resident and there are a lot of dirty lines
   }
}
// -------------------------------------------------------------------------------------
NvmBufferFrame *FullBufferFrame::FlushShadow(NvmBufferFrame *new_nvm_bf, ub8 pvn)
{
   assert(nvm_buffer_frame != nullptr && nvm_buffer_frame != new_nvm_bf);
   assert(IsAnythingDirty());

   // Swap volatile ptr in the nvm bf (page table is updated by the buffer manager)
   NvmBufferFrame *old_nvm_bf = nvm_buffer_frame;
   nvm_buffer_frame = new_nvm_bf;

   // 1. Persist data to new page
   if (all_resident || resident.all()) {
      // Best case: everything is in ram -> copy from dram
      for (ub4 cl_id = 0; cl_id<constants::kCacheLinesPerPage; cl_id++) {
         ub1 *src = aligned_page.GetPage().Ptr() + (cl_id * constants::kCacheLineByteCount);
         ub1 *dest = new_nvm_bf->GetPage().Ptr() + (cl_id * constants::kCacheLineByteCount);
         pf_utils::alex_CopyAndFlush512(dest, src);
      }
   } else {
      assert(!all_dirty && !dirty.all()); // -> otherwise, everything would have been resident as well

      // Otherwise:
      //  1. Copy non-resident data from old nvm to new nvm (because not everything is in dram)
      //  2. Write resident data directly from ram to new nvm
      for (ub4 cl_id = 0; cl_id<constants::kCacheLinesPerPage; cl_id += 4) {
         ub1 *src0 = (resident.test(cl_id + 0) ? aligned_page.GetPage().Ptr() : old_nvm_bf->GetPage().Ptr()) + ((cl_id + 0) * constants::kCacheLineByteCount);
         ub1 *dest0 = new_nvm_bf->GetPage().Ptr() + ((cl_id + 0) * constants::kCacheLineByteCount);
         pf_utils::alex_CopyAndFlush512(dest0, src0);

         ub1 *src1 = (resident.test(cl_id + 1) ? aligned_page.GetPage().Ptr() : old_nvm_bf->GetPage().Ptr()) + ((cl_id + 1) * constants::kCacheLineByteCount);
         ub1 *dest1 = new_nvm_bf->GetPage().Ptr() + ((cl_id + 1) * constants::kCacheLineByteCount);
         pf_utils::alex_CopyAndFlush512(dest1, src1);

         ub1 *src2 = (resident.test(cl_id + 2) ? aligned_page.GetPage().Ptr() : old_nvm_bf->GetPage().Ptr()) + ((cl_id + 2) * constants::kCacheLineByteCount);
         ub1 *dest2 = new_nvm_bf->GetPage().Ptr() + ((cl_id + 2) * constants::kCacheLineByteCount);
         pf_utils::alex_CopyAndFlush512(dest2, src2);

         ub1 *src3 = (resident.test(cl_id + 3) ? aligned_page.GetPage().Ptr() : old_nvm_bf->GetPage().Ptr()) + ((cl_id + 3) * constants::kCacheLineByteCount);
         ub1 *dest3 = new_nvm_bf->GetPage().Ptr() + ((cl_id + 3) * constants::kCacheLineByteCount);
         pf_utils::alex_CopyAndFlush512(dest3, src3);
      }
   }
   alex_SFence();

   // 2. Mark new page as valid
   new_nvm_bf->dirty = true;
   new_nvm_bf->page_id = page_id;
   alex_SFence(); // Able to avoid flush because of pvn trick (see paper)
   new_nvm_bf->pvn = pvn;
   alex_WriteBack(new_nvm_bf, constants::kCacheLineByteCount);
   alex_SFence();

   // Done
   all_dirty = false;
   dirty.reset();
   return old_nvm_bf;
}
// -------------------------------------------------------------------------------------
void FullBufferFrame::WriteToNvmComplete()
{
   assert(nvm_buffer_frame != nullptr);

   memcpy(nvm_buffer_frame->GetPage().Ptr(), aligned_page.GetPage().Ptr(), constants::kPageByteCount);
   pmem_persist(nvm_buffer_frame->GetPage().Ptr(), constants::kPageByteCount);
   nvm_buffer_frame->dirty = true;
   all_dirty = false;
   dirty.reset();
}
// -------------------------------------------------------------------------------------
void FullBufferFrame::WriteToNvmPartial()
{
   assert(nvm_buffer_frame != nullptr);

   for (ub4 cacheLineId = 0; cacheLineId<constants::kCacheLinesPerPage; ++cacheLineId) { // TODO opt loop by using ranges ?
      if (dirty.test(cacheLineId)) {
         memcpy(nvm_buffer_frame->GetPage().Ptr() + (cacheLineId * constants::kCacheLineByteCount), aligned_page.GetPage().Ptr() + (cacheLineId * constants::kCacheLineByteCount), constants::kCacheLineByteCount);
         pmem_flush(nvm_buffer_frame->GetPage().Ptr() + (cacheLineId * constants::kCacheLineByteCount), constants::kCacheLineByteCount);
      }
   }
   nvm_buffer_frame->dirty = true;
   pmem_drain();
   all_dirty = false;
   dirty.reset();
}
// -------------------------------------------------------------------------------------
void FullBufferFrame::SwapOut()
{
   assert(!all_dirty);
   assert(dirty.none());
   page_id = constants::kInvalidPageId;
   all_resident = false;
   resident.reset();
   nvm_buffer_frame = nullptr;
}
// -------------------------------------------------------------------------------------
template<EnsureMode mode>
ub1 *FullBufferFrame::Translate()
{
   switch (mode) {
      case EnsureMode::Ensure:EnsureResidency();
         return RamPtr<ub1>();
      case EnsureMode::DoNothing:return RamPtr<ub1>();
      case EnsureMode::MarkDirty:MarkDirty();
         return RamPtr<ub1>();
      case EnsureMode::EnsureAndDirty:EnsureResidency();
         MarkDirty();
         return RamPtr<ub1>();
   }
   throw;
}
// -------------------------------------------------------------------------------------
template<EnsureMode mode>
ub1 *FullBufferFrame::Translate(ub4 offset, ub4 size)
{
   assert(offset + size<=constants::kPageByteCount);
   switch (mode) {
      case EnsureMode::Ensure:return EnsureResidency(offset, size);
      case EnsureMode::DoNothing:return RamPtr<ub1>() + offset;
      case EnsureMode::MarkDirty:return MarkDirty(offset, size);
      case EnsureMode::EnsureAndDirty:EnsureResidency(offset, size);
         return MarkDirty(offset, size);
   }
   throw;
}
// -------------------------------------------------------------------------------------
class PageFlusher : public Worker {
   Random ranny;

   ub8 page_count;
   string nvm_file;
   vector<double> nano_seconds;

   std::unique_ptr<VolatileMemory> ram;
   std::unique_ptr<NonVolatileMemory> nvm; // One more page than in dram, because of shadow
   unordered_map<ub4, NvmBufferFrame *> nvm_mapping;

   NvmBufferFrame *free_nvm_bf;

public:
   PageFlusher(const string &nvm_file, ub8 page_count, ub4 tid, string config)
           : Worker(tid, config)
             , page_count(page_count)
             , nvm_file(nvm_file)
   {
   }

   void Run()
   {
      Setup();
      ready = true;
      while (!run) {
         this_thread::yield();
      }

      while (!stop) {
         auto begin = chrono::high_resolution_clock::now();
         FlushAll();
         auto end = chrono::high_resolution_clock::now();
         nano_seconds.push_back(chrono::duration_cast<chrono::nanoseconds>(end - begin).count());
         performed_iteration_count++;
      }
   }

   void PrintResultOfLastIteration(ub4 iteration)
   {
      if (!stop || iteration>=performed_iteration_count) {
         throw;
      }
      double ns = nano_seconds[iteration];
      //@formatter:off
      cout << "RES page_flusher " << config
           << " tid= " << tid
           << " iterations: " << iteration << "/" << performed_iteration_count
           << " perf(pages/s): " << ub8(page_count / (ns / 1e9)) << endl;
      //@formatter:on
   }

   PageFlusher(const PageFlusher &) = delete;
   PageFlusher &operator=(const PageFlusher &) = delete;

private:
   NvmBufferFrame *GetMappedNvmBf(ub8 id) { return nvm_mapping.find(id)->second; }
   FullBufferFrame *GetRamBf(ub8 id) { return ram->GetPtr<FullBufferFrame>(id); }

   // Set nvm and dram content to 'x'
   void Setup()
   {
      ram = make_unique<VolatileMemory>(page_count * sizeof(FullBufferFrame));
      nvm = make_unique<NonVolatileMemory>(nvm_file, (page_count + 10) * sizeof(NvmBufferFrame)); // + 10 to have some space after the memory for the pages to put the micro log or the shadow-copy page of CoW

      memset(ram->Data(), 'x', ram->GetByteCount());
      memset(nvm->Data(), 'x', nvm->GetByteCount());
      for (ub4 p = 0; p<page_count; ++p) {
         NvmBufferFrame *nvm_bf = &nvm->GetNvmBufferFrame(p);
         nvm_bf->init();
         nvm_bf->page_id = p;
         GetRamBf(p)->Init();
         GetRamBf(p)->SwapIn(p, nvm_bf);
         nvm_mapping[p] = nvm_bf;
      }

      // All techniques (cow and micro log) need a small buffer, we put this after all regular pages
      // The buffers also overlap because only one of them is used in a given experiment
      // Kind of a hacky design, but good enough for some benchmark code
      free_nvm_bf = &nvm->GetNvmBufferFrame(page_count);
   }

   void FlushAll()
   {
      ub8 lsn = 0;
      free_nvm_bf->init();
      free_nvm_bf->page_id = constants::kInvalidPageId;

      for (ub4 p = 0; p<page_count; ++p) {
         GetRamBf(p)->all_dirty = true;
         GetRamBf(p)->all_resident = true;
         NvmBufferFrame *new_free_one = GetRamBf(p)->FlushShadow(free_nvm_bf, lsn++);
         assert(GetRamBf(p)->GetNvmBufferFrame() == free_nvm_bf);
         free_nvm_bf = new_free_one;
         nvm_mapping[p] = GetRamBf(p)->GetNvmBufferFrame();
      }
   }
};
// -------------------------------------------------------------------------------------

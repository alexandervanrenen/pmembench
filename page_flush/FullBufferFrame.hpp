#pragma once
// -------------------------------------------------------------------------------------
#include "Common.hpp"
#include "Pages.hpp"
#include <immintrin.h>
#include <array>
#include <bitset>
#include <cstring>
#include <iostream>
// -------------------------------------------------------------------------------------
struct MicroLog {
   // Cache line 0 to 4
   ub8 page_id;
   ub4 count;
   std::array<ub1, constants::kCacheLinesPerPage> mapping;

   std::array<ub1, constants::kCacheLineByteCount - 12> padding;

   // Cache line 5 to 261
   std::array<std::array<ub1, constants::kCacheLineByteCount>, constants::kCacheLinesPerPage> cls;
};
static_assert(sizeof(MicroLog)<=sizeof(NvmBufferFrame), "");
static_assert(sizeof(MicroLog) == (5 + constants::kCacheLinesPerPage) * constants::kCacheLineByteCount, "");
// -------------------------------------------------------------------------------------
struct MicroLog2 {
   // Cache line 0 to 4
   ub8 page_id;
   ub4 bit_count;
   ub4 count;
   std::array<ub1, constants::kCacheLinesPerPage> mapping;

   std::array<ub1, constants::kCacheLineByteCount - 16> padding;

   // Cache line 5 to 261
   std::array<std::array<ub1, constants::kCacheLineByteCount>, constants::kCacheLinesPerPage> cls;
};
static_assert(sizeof(MicroLog2)<=sizeof(NvmBufferFrame), "");
static_assert(sizeof(MicroLog2) == (5 + constants::kCacheLinesPerPage) * constants::kCacheLineByteCount, "");
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
   inline void FlushMicroLog(MicroLog &log);
   inline void FlushMicroLog(MicroLog2 &log);
   inline void WriteToNvmComplete();
   inline void WriteToNvmPartial();
   inline void SwapOut();

   friend class FlushTest;
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
         alex_CopyAndFlush512(dest, src);
      }
   } else {
      assert(!all_dirty && !dirty.all()); // -> otherwise, everything would have been resident as well

      // Otherwise:
      //  1. Copy old data from old nvm dram (because not everything is in dram)
      //  2. Do partial flush (necessary, because we would overwrite something)
      for (ub4 cl_id = 0; cl_id<constants::kCacheLinesPerPage; cl_id += 4) {
         ub1 *src0 = (resident.test(cl_id + 0) ? aligned_page.GetPage().Ptr() : old_nvm_bf->GetPage().Ptr()) + ((cl_id + 0) * constants::kCacheLineByteCount);
         ub1 *dest0 = new_nvm_bf->GetPage().Ptr() + ((cl_id + 0) * constants::kCacheLineByteCount);
         alex_CopyAndFlush512(dest0, src0);

         ub1 *src1 = (resident.test(cl_id + 1) ? aligned_page.GetPage().Ptr() : old_nvm_bf->GetPage().Ptr()) + ((cl_id + 1) * constants::kCacheLineByteCount);
         ub1 *dest1 = new_nvm_bf->GetPage().Ptr() + ((cl_id + 1) * constants::kCacheLineByteCount);
         alex_CopyAndFlush512(dest1, src1);

         ub1 *src2 = (resident.test(cl_id + 2) ? aligned_page.GetPage().Ptr() : old_nvm_bf->GetPage().Ptr()) + ((cl_id + 2) * constants::kCacheLineByteCount);
         ub1 *dest2 = new_nvm_bf->GetPage().Ptr() + ((cl_id + 2) * constants::kCacheLineByteCount);
         alex_CopyAndFlush512(dest2, src2);

         ub1 *src3 = (resident.test(cl_id + 3) ? aligned_page.GetPage().Ptr() : old_nvm_bf->GetPage().Ptr()) + ((cl_id + 3) * constants::kCacheLineByteCount);
         ub1 *dest3 = new_nvm_bf->GetPage().Ptr() + ((cl_id + 3) * constants::kCacheLineByteCount);
         alex_CopyAndFlush512(dest3, src3);
      }
   }
   alex_SFence();

   // 2. Mark new page as valid
   new_nvm_bf->dirty = true;
   new_nvm_bf->page_id = page_id;
   alex_SFence();
   new_nvm_bf->pvn = pvn;
   alex_WriteBack(new_nvm_bf, constants::kCacheLineByteCount);
   alex_SFence();

   // Done
#ifndef KEEP_DIRTY
   all_dirty = false;
   dirty.reset();
#endif
   return old_nvm_bf;
}
// -------------------------------------------------------------------------------------
void FullBufferFrame::FlushMicroLog(MicroLog &log)
{
   assert(log.page_id == constants::kInvalidPageId);
   assert(log.count == 0);
   assert(IsAnythingDirty());

   // 1. Write data to log
   for (ub4 cl_id = 0; cl_id<constants::kCacheLinesPerPage; cl_id++) {
      if (dirty.test(cl_id)) {
         ub1 *src = aligned_page.GetPage().Ptr() + (cl_id * constants::kCacheLineByteCount);
         ub1 *dest = log.cls[log.count].data();
         alex_CopyAndFlush512(dest, src);
         log.mapping[log.count] = cl_id;
         log.count++;
      }
   }
   alex_WriteBack(&log, log.count + 12);
   alex_SFence();

   // 2. Set log valid
   log.page_id = page_id;
   alex_WriteBack(&log, constants::kCacheLineByteCount);
   alex_SFence();

   // 3. Put data into page
   for (ub4 cl_id = 0; cl_id<constants::kCacheLinesPerPage; cl_id++) {
      if (dirty.test(cl_id)) {
         ub1 *src = aligned_page.GetPage().Ptr() + (cl_id * constants::kCacheLineByteCount);
         ub1 *dest = nvm_buffer_frame->GetPage().Ptr() + (cl_id * constants::kCacheLineByteCount);
         alex_CopyAndFlush512(dest, src);
      }
   }
   nvm_buffer_frame->dirty = true;
   alex_SFence();
#ifndef KEEP_DIRTY
   dirty.reset();
   all_dirty = false;
#endif

   // 4. Set log invalid
   log.page_id = constants::kInvalidPageId;
   log.count = 0;
   alex_WriteBack(&log);
   alex_SFence();
}
// -------------------------------------------------------------------------------------
void FullBufferFrame::FlushMicroLog(MicroLog2 &log)
{
   assert(log.page_id == constants::kInvalidPageId);
   assert(log.count == 0);
   assert(log.bit_count == 0);
   assert(IsAnythingDirty());

   // 1. Write data to log
   ub8 bit_count = 0;
   for (ub4 cl_id = 0; cl_id<constants::kCacheLinesPerPage; cl_id++) {
      if (dirty.test(cl_id)) {
         ub1 *src = aligned_page.GetPage().Ptr() + (cl_id * constants::kCacheLineByteCount);
         ub1 *dest = log.cls[log.count].data();
         memcpy(dest, src, constants::kCacheLineByteCount);
         alex_WriteBack(dest, constants::kCacheLineByteCount);
         bit_count += alex_PopCount(src, constants::kCacheLineByteCount);
         log.mapping[log.count] = cl_id;
         log.count++;
      }
   }
   log.page_id = page_id;
   bit_count += alex_PopCount(&log, constants::kCacheLineByteCount * 5);
   log.bit_count = bit_count;
   alex_WriteBack(&log, log.count + 16);
   alex_SFence();

   // 2. Put data into page
   for (ub4 cl_id = 0; cl_id<constants::kCacheLinesPerPage; cl_id++) {
      if (dirty.test(cl_id)) {
         ub1 *src = aligned_page.GetPage().Ptr() + (cl_id * constants::kCacheLineByteCount);
         ub1 *dest = nvm_buffer_frame->GetPage().Ptr() + (cl_id * constants::kCacheLineByteCount);
         memcpy(dest, src, constants::kCacheLineByteCount);
         alex_FlushOpt(dest, constants::kCacheLineByteCount);
      }
   }
   nvm_buffer_frame->dirty = true;
   alex_SFence();
   dirty.reset();
   all_dirty = false;

   // 3. Set log invalid
   memset(&log, 0, constants::kCacheLineByteCount);
   alex_WriteBack(&log, constants::kCacheLineByteCount);
   alex_SFence();
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

#pragma once
// -------------------------------------------------------------------------------------
#include "Common.hpp"
#include <list>
#include <array>
// -------------------------------------------------------------------------------------
class Page {
public:
   template<class T = ub1>
   T *Ptr() { return reinterpret_cast<T *>( data.data()); }

   template<class T = ub1>
   const T *Ptr() const { return reinterpret_cast<const T *>( data.data()); }

private:
   std::array<ub1, constants::kPageByteCount> data;
};
// -------------------------------------------------------------------------------------
template<ub8 ALIGNMENT>
class AutoAlignedPage {
public:
   Page &GetPage()
   {
      assert(padding.data()<data.data());
      auto res = (ub1 *) (((uintptr_t) data.data()) & ~(ALIGNMENT - 1));
      assert(IsAlignedAt<ALIGNMENT>(res));
      return *reinterpret_cast<Page *>( res);
   }

   const Page &GetPage() const
   {
      assert(padding.data()<data.data());
      auto res = (const ub1 *) (((uintptr_t) data.data()) & ~(ALIGNMENT - 1));
      assert(IsAlignedAt<ALIGNMENT>(res));
      return *reinterpret_cast<const Page *>( res);
   }

private:
   std::array<ub1, ALIGNMENT> padding;
   std::array<ub1, constants::kPageByteCount> data;
};
// -------------------------------------------------------------------------------------
static_assert(sizeof(Page) == constants::kPageByteCount, "Pages are always 16kb");
// -------------------------------------------------------------------------------------
class NvmBufferFrame {
public:
   inline void init()
   {
      dirty = false;
      page_id = constants::kInvalidPageId;
      pvn = 0;
   }

   Page &GetPage() { return page.GetPage(); }
   const Page &GetPage() const { return page.GetPage(); }

   bool dirty;
   ub8 page_id;
   ub8 pvn;

private:
   AutoAlignedPage<constants::kPageAlignment> page;
};
// -------------------------------------------------------------------------------------
static_assert(sizeof(NvmBufferFrame) % 8 == 0, "NvmBufferFrame should be eight byte aligned.");
// -------------------------------------------------------------------------------------

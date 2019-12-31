#pragma once
// -------------------------------------------------------------------------------------
#include "Common.hpp"
// -------------------------------------------------------------------------------------
class VolatileMemory {

public:
   VolatileMemory(ub8 byte_count);
   ~VolatileMemory();
   VolatileMemory(const VolatileMemory &) = delete;
   VolatileMemory &operator=(const VolatileMemory &) = delete;

   ub1 *Data() { return data_ptr; }
   ub1 *End() { return data_ptr + byte_count; }

   ub8 GetByteCount() const { return byte_count; }

   template<class T>
   T *GetPtr(ub8 offset = 0) { return reinterpret_cast<T *>(data_ptr) + offset; }
   ub1 *GetPtr(ub8 offset = 0) { return data_ptr + offset; }

private:
   ub1 *const data_ptr;
   const ub8 byte_count;
};
// -------------------------------------------------------------------------------------
VolatileMemory::VolatileMemory(ub8 byte_count)
        : data_ptr(new ub1[byte_count])
          , byte_count(byte_count)
{
   assert(byte_count != 0);
}
// -------------------------------------------------------------------------------------
VolatileMemory::~VolatileMemory()
{
   delete[] data_ptr;
}
// -------------------------------------------------------------------------------------
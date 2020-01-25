#pragma once
// -------------------------------------------------------------------------------------
#include "Common.hpp"
#include <cstdlib>
#include <string>
#include <inttypes.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <libpmem.h>
// -------------------------------------------------------------------------------------
class NonVolatileMemory {
public:
   NonVolatileMemory(ub8 byte_count); // Uses dram that is not file backed
   NonVolatileMemory(const std::string &file_name, ub8 byte_count); // Uses dram or nvm depending on the file path
   NonVolatileMemory(const NonVolatileMemory &) = delete;
   NonVolatileMemory &operator=(const NonVolatileMemory &) = delete;

   ~NonVolatileMemory();

   ub1 *Data() { return data_ptr; }
   ub1 *End() { return data_ptr + byte_count; }
   ub8 GetByteCount() { return byte_count; }

   void FlushAll();
   void Flush(ub8 from, ub8 length);

   NvmBufferFrame &GetNvmBufferFrame(ub8 id)
   {
      assert(data_ptr + id * sizeof(NvmBufferFrame)<=End());
      assert(reinterpret_cast<NvmBufferFrame *>(data_ptr)[id].GetPage().Ptr() != nullptr);
      return reinterpret_cast<NvmBufferFrame *>(data_ptr)[id];
   }

   bool IsRealNvm() const { return is_real_nvm; }

private:
   ub1 *data_ptr;
   std::string file_name;
   const ub8 byte_count;
   bool is_real_nvm;
   bool is_mapped_file;
};
// -------------------------------------------------------------------------------------
NonVolatileMemory::NonVolatileMemory(ub8 byte_count)
        : byte_count(byte_count)
          , is_mapped_file(false)
{
   assert(((ub8) ((off_t) byte_count)) == byte_count);
   assert(byte_count>0);

   data_ptr = AlignedAlloc(512, byte_count);

   is_real_nvm = false;
}
// -------------------------------------------------------------------------------------
NonVolatileMemory::NonVolatileMemory(const std::string &file_name, ub8 byte_count)
        : file_name(file_name)
          , byte_count(byte_count)
          , is_mapped_file(true)
{
   assert(((ub8) ((off_t) byte_count)) == byte_count);

   // No need to do anything if zero byte are requested; Does this happen ??
   assert(byte_count>0); // XXX
   if (byte_count == 0) {
      data_ptr = nullptr;
      return;
   }

   // Map the file (our pmem wrapper works with normal memory by falling back to mmap)
   size_t acquired_byte_count;
   data_ptr = reinterpret_cast<ub1 *>(pmem_map_file(file_name.c_str(), byte_count, PMEM_FILE_CREATE, 0666, &acquired_byte_count, nullptr));
   if (data_ptr == nullptr) {
      std::cout << "Failed to create file: '" << file_name << "'." << std::endl;
      throw;
   }
   if (acquired_byte_count != byte_count) {
      std::cout << "Failed to allocate requested size for file: '" << file_name << "'. (Requested=" << byte_count << ", Aquired=" << acquired_byte_count << ")" << std::endl;
      throw;
   }

   // Do this only once, as it is expensive
   is_real_nvm = pmem_is_pmem(data_ptr, 1);
}
// -------------------------------------------------------------------------------------
NonVolatileMemory::~NonVolatileMemory()
{
   if (is_mapped_file) {
      pmem_unmap(data_ptr, byte_count);
   } else {
      free(data_ptr);
   }
}
// -------------------------------------------------------------------------------------
void NonVolatileMemory::FlushAll()
{
   if (is_mapped_file) {
      pmem_persist(data_ptr, byte_count);
   }
}
// -------------------------------------------------------------------------------------
void NonVolatileMemory::Flush(ub8 from, ub8 length)
{
   if (is_mapped_file) {
      pmem_persist(data_ptr + from, length);
   }
}
// -------------------------------------------------------------------------------------

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

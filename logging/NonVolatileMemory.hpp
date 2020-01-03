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

   bool IsNvm() const { return is_nvm; }

private:
   ub1 *data_ptr;
   std::string file_name;
   const ub8 byte_count;
   bool is_nvm;
   bool is_mapped_file;
   int file_fd;
};
// -------------------------------------------------------------------------------------
NonVolatileMemory::NonVolatileMemory(ub8 byte_count)
        : byte_count(byte_count)
          , is_mapped_file(false)
{
   assert(((ub8) ((off_t) byte_count)) == byte_count);

   assert(byte_count>0); // XXX

   data_ptr = AlignedAlloc(512, byte_count);

   is_nvm = false;
}
// -------------------------------------------------------------------------------------
NonVolatileMemory::NonVolatileMemory(const std::string &file_name, ub8 byte_count)
        : file_name(file_name)
          , byte_count(byte_count)
          , is_nvm(true)
          , is_mapped_file(true)
{
   assert(((ub8) ((off_t) byte_count)) == byte_count);

   file_fd = open(file_name.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
   int td = ftruncate(file_fd, byte_count);
   if (file_fd<0 || td<0) {
      std::cout << "unable to create file" << std::endl;
      exit(-1);
   }
   data_ptr = (ub1 *) mmap(nullptr, byte_count, PROT_WRITE, MAP_SHARED, file_fd, 0);
}
// -------------------------------------------------------------------------------------
NonVolatileMemory::~NonVolatileMemory()
{
   // Benchmark code .. dont care ;p
}
// -------------------------------------------------------------------------------------

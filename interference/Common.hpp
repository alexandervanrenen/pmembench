#pragma once
// -------------------------------------------------------------------------------------
#include <immintrin.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include <mutex>
#include <atomic>
#include "libpmem.h"
// -------------------------------------------------------------------------------------
using ub1 = uint8_t;
using ub2 = uint16_t;
using ub4 = uint32_t;
using ub8 = uint64_t;
// -------------------------------------------------------------------------------------
using sb1 = int8_t;
using sb2 = int16_t;
using sb4 = int32_t;
using sb8 = int64_t;
// -------------------------------------------------------------------------------------
namespace constants {
const static ub4 kCacheLineByteCount = 64; // 64 Byte cache lines
const static ub4 kPageByteCount = 1 << 14; // 16 KB
const static ub4 kCacheLinesPerPage = kPageByteCount / kCacheLineByteCount; // 16KB/64Byte
const static ub4 kPageAlignment = 512; // For O_Direct
const static ub8 kInvalidPageId = ~0;
}
// -------------------------------------------------------------------------------------
static std::mutex global_io_mutex;
// -------------------------------------------------------------------------------------
#define a_mm_clflush(addr)\
    asm volatile("clflush %0" : "+m" (*(volatile char *)addr));
#define a_mm_clflushopt(addr)\
    asm volatile(".byte 0x66; clflush %0" : "+m" (*(volatile char *)addr));
#define a_mm_clwb(addr)\
    asm volatile(".byte 0x66; xsaveopt %0" : "+m" (*(volatile char *)addr));
#define a_mm_pcommit()\
    asm volatile(".byte 0x66, 0x0f, 0xae, 0xf8");
// -------------------------------------------------------------------------------------
// Based on: https://en.wikipedia.org/wiki/Xorshift
class Random {
public:
   explicit Random(ub8 seed = 2305843008139952128ull) // The 8th perfect number found 1772 by Euler with <3
           : seed(seed)
   {
   }

   uint64_t Rand()
   {
      seed ^= (seed << 13);
      seed ^= (seed >> 7);
      return (seed ^= (seed << 17));
   }

   ub8 seed;
};
// -------------------------------------------------------------------------------------
template<uint32_t byteCount>
bool IsAlignedAt(const void *ptr)
{
   return ((uint64_t) ptr) % byteCount == 0;
}
// -------------------------------------------------------------------------------------
uint8_t *AlignedAlloc(uint64_t alignment, uint64_t size)
{
   void *result = nullptr;
   int error = posix_memalign(&result, alignment, size);
   if (error) {
      std::cout << "error while allocating" << std::endl;
      throw;
   }
   return reinterpret_cast<uint8_t *>(result);
}
// -------------------------------------------------------------------------------------
inline void alex_FlushOpt(void *addr)
{
   a_mm_clflushopt((char *) addr);
}
// -------------------------------------------------------------------------------------
inline void alex_WriteBack(void *addr, ub4 len)
{
   for (uintptr_t uptr = (uintptr_t) addr & ~(64 - 1); uptr<(uintptr_t) addr + len; uptr += 64) {
      a_mm_clwb((char *) uptr);
   }
}
// -------------------------------------------------------------------------------------
inline void alex_WriteBack(void *addr)
{
   addr = (ub1 *) ((uintptr_t) addr & ~(64 - 1));
   a_mm_clwb((char *) addr);
}
// -------------------------------------------------------------------------------------
inline void alex_SFence()
{
   _mm_sfence();
}
// -------------------------------------------------------------------------------------
struct Worker {
   Worker(ub4 tid, std::string config)
           : tid(tid)
             , config(config) {}

   ub4 tid;
   std::string config;
   std::atomic<bool> run = false;
   std::atomic<bool> ready = false;
   std::atomic<bool> stop = false;
   std::atomic<ub4> performed_iteration_count = 0;

   virtual void PrintResultOfLastIteration(ub4 iteration) = 0;
};
// -------------------------------------------------------------------------------------

#pragma once
// -------------------------------------------------------------------------------------
#include <immintrin.h>
#include <cassert>
#include <cstring>
#include <iostream>
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
#define a_mm_clflush(addr)\
    asm volatile("clflush %0" : "+m" (*(volatile char *)addr));
#define a_mm_clflushopt(addr)\
    asm volatile(".byte 0x66; clflush %0" : "+m" (*(volatile char *)addr));
#define a_mm_clwb(addr)\
    asm volatile(".byte 0x66; xsaveopt %0" : "+m" (*(volatile char *)addr));
#define a_mm_pcommit()\
    asm volatile(".byte 0x66, 0x0f, 0xae, 0xf8");
// -------------------------------------------------------------------------------------
inline void alex_WriteBack(void *addr, ub4 len)
{
   for (uintptr_t uptr = (uintptr_t) addr & ~(64 - 1); uptr<(uintptr_t) addr + len; uptr += 64)
           a_mm_clwb((char *) uptr);
}
// -------------------------------------------------------------------------------------
inline void alex_WriteBack(void *addr)
{
   addr = (ub1 *) ((uintptr_t) addr & ~(64 - 1));
   a_mm_clwb((char *) addr);
}
// -------------------------------------------------------------------------------------
inline void alex_FlushOpt(void *addr, ub4 len)
{
   for (uintptr_t uptr = (uintptr_t) addr & ~(64 - 1); uptr<(uintptr_t) addr + len; uptr += 64)
           a_mm_clflushopt((char *) uptr);
}
// -------------------------------------------------------------------------------------
inline void alex_FlushOpt(void *addr)
{
   a_mm_clflushopt((char *) addr);
}
// -------------------------------------------------------------------------------------
inline void alex_SFence()
{
   _mm_sfence();
}
// -------------------------------------------------------------------------------------
inline int numberOfSetBits(uint32_t i)
{
   i = i - ((i >> 1) & 0x55555555);
   i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
   return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}
// -------------------------------------------------------------------------------------
inline ub8 alex_PopCount(ub8 value)
{
#ifdef __APPLE__
   return numberOfSetBits(value & ((1ull << 32) - 1)) + numberOfSetBits(value >> 32);
#else
   return _mm_popcnt_u64(value);
#endif
}
// -------------------------------------------------------------------------------------
inline ub8 alex_PopCount(void *addr_in, ub4 len)
{
   assert(len % 8 == 0);
   ub8 *addr = (ub8 *) addr_in;

   ub8 result = 0;
   for (ub4 i = 0; i<len; i += 8) {
      alex_PopCount(addr[i / 8]);
   }
   return result;
}
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

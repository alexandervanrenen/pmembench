#pragma once
// -------------------------------------------------------------------------------------
#include <cstdint>
#include <iostream>
#include <array>
#include <immintrin.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include "libpmem.h"
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
inline void alex_FlushOpt(void *addr, ub4 len)
{
   for (uintptr_t uptr = (uintptr_t) addr & ~(64 - 1); uptr<(uintptr_t) addr + len; uptr += 64) {
      a_mm_clflushopt((char *) uptr);
   }
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
inline ub8 alex_PopCount(ub8 value)
{
   return _mm_popcnt_u64(value);
}
// -------------------------------------------------------------------------------------
inline void alex_FlushClToNvm(void *dest, void *src)
{
   assert(((ub8) dest) % 64 == 0);
   assert(((ub8) src) % 64 == 0);
   __m512i reg = _mm512_load_si512(src);
   _mm512_stream_si512((__m512i *) dest, reg);
}
// -------------------------------------------------------------------------------------
void FastCopy512(ub1 *dest, const ub1 *src)
{
   assert(((ub8) dest) % 64 == 0);
   memcpy(dest, src, 64);
}
// -------------------------------------------------------------------------------------
ub4 FastPopCount512(const ub1 *ptr)
{
   ub4 res = 0;
   for (ub4 i = 0; i<64; i += 8) {
      res += alex_PopCount(*(ub8 *) (&ptr[i]));
   }
   return res;
}
// -------------------------------------------------------------------------------------
void FastCopy512Simd(ub1 *dest, const ub1 *src)
{
   assert(((ub8) dest) % 64 == 0);
   __m512i reg = _mm512_loadu_si512(src);
   _mm512_store_si512((__m512i *) dest, reg);
}
// -------------------------------------------------------------------------------------
void alex_FastCopyAndWriteBack(ub1 *nvm_begin, const ub1 *ram_begin, ub4 size)
{
   assert(size>0);

   // Copy head bytes
   ub4 pos = 0;
   ub8 off = (ub8) nvm_begin % 64;
   if (off != 0) {
      ub8 byte_count = (64 - off)>size ? size : (64 - off);
      memcpy(nvm_begin, ram_begin, byte_count);
      alex_WriteBack(nvm_begin);
      pos = byte_count;
   }

   // Copy full cache lines (and flush)
   for (; pos + 63<size; pos += 64) {
      assert(((ub8) nvm_begin + pos) % 64 == 0);
      memcpy(nvm_begin + pos, ram_begin + pos, 64);
      alex_WriteBack(nvm_begin + pos);
   }

   // Copy remaining bytes (and flush)
   if (pos<size) {
      memcpy(nvm_begin + pos, ram_begin + pos, size - pos);
      alex_WriteBack(nvm_begin + pos);
      pos = size;
   }

   assert(pos == size && "aahhhhh");
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
// Based on: https://en.wikipedia.org/wiki/Xorshift
class Random {
public:
   explicit Random(uint64_t seed = 2305843008139952128ull) // The 8th perfect number found 1772 by Euler with <3
           : seed(seed)
   {
   }

   uint64_t Rand()
   {
      seed ^= (seed << 13);
      seed ^= (seed >> 7);
      return (seed ^= (seed << 17));
   }

   uint64_t seed;
};
// -------------------------------------------------------------------------------------
void DumpHex(const void *data_in, uint32_t size, std::ostream &os)
{
   char buffer[16];

   const char *data = reinterpret_cast<const char *>(data_in);
   for (uint32_t i = 0; i<size; i++) {
      sprintf(buffer, "%02hhx", data[i]);
      os << buffer[0] << buffer[1] << " ";
   }
}
// -------------------------------------------------------------------------------------
template<class TARGET, class SOURCE>
inline TARGET Cast(SOURCE *ptr) { return reinterpret_cast<TARGET>(ptr); }
// -------------------------------------------------------------------------------------
char *CreateAlignedString(Random &ranny, uint32_t len)
{
   char *data = (char *) malloc(len + 1);
   assert((uint64_t) data % 4 == 0);

   for (uint32_t i = 0; i<len; i++) {
      data[i] = ranny.Rand() % 256;
   }
   data[len] = '\0';

   return data;
}
// -------------------------------------------------------------------------------------

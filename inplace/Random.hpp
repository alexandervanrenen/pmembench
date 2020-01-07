#pragma once
// -------------------------------------------------------------------------------------
#include <cstdint>
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

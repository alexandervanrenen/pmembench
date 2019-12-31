#pragma once
// -------------------------------------------------------------------------------------
#include "Common.hpp"
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

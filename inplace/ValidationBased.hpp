#include "Common.hpp"
#include <memory>
#include <vector>
// -------------------------------------------------------------------------------------
template<uint32_t entry_size>
struct ValidationBased {
   uint64_t entry_count;
   std::vector <Operation<entry_size>> data;

   ValidationBased(const std::string &, uint64_t entry_count)
           : entry_count(entry_count)
           , data(entry_count)
   {
      memset(data.data(), 'a', sizeof(Operation<entry_size>) * entry_count);
   }

   void DoUpdate(const Operation<entry_size> &op, uint32_t id)
   {
      assert(id<entry_count);
      data[id] = op;
   }

   uint64_t ReadSingleResult(Operation<entry_size> &result, uint32_t id)
   {
      result = data[id];
      return result.entry_id;
   }
};
// -------------------------------------------------------------------------------------

#include "Common.hpp"
#include "NonVolatileMemory.hpp"
#include <memory>
// -------------------------------------------------------------------------------------
namespace cow {
// -------------------------------------------------------------------------------------
template<uint32_t BYTE_COUNT>
struct InplaceCow {
   alignas(64) bool is_a_active; // Start at a cl to allow the use of streaming ops
   alignas(64) std::array<char, BYTE_COUNT> a;
   alignas(64) std::array<char, BYTE_COUNT> b; // Start at a cl to allow the use of streaming ops

   void Write(const char *input)
   {
      bool is_a_active_cache = is_a_active;
      if (is_a_active_cache) {
         alex_FastCopyAndWriteBack((ub1 *) b.data(), (const ub1 *) input, BYTE_COUNT);
         alex_SFence();
      } else {
         alex_FastCopyAndWriteBack((ub1 *) a.data(), (const ub1 *) input, BYTE_COUNT);
         alex_SFence();
      }

      is_a_active = !is_a_active_cache;
      alex_WriteBack(&is_a_active);
      alex_SFence();
   }

   void Read(char *output)
   {
      if (is_a_active) {
         memcpy(output, a.data(), BYTE_COUNT);
      } else {
         memcpy(output, b.data(), BYTE_COUNT);
      }
   }

   void Init(Random &ranny)
   {
      is_a_active = true; // ranny.Rand() % 2; // Make it so that the branch is ~50%
   }
};
// -------------------------------------------------------------------------------------
template<uint32_t entry_size>
struct CowBasedUpdates {
   NonVolatileMemory nvm_data;
   uint64_t entry_count;
   InplaceCow<entry_size> *entries;

   CowBasedUpdates(const std::string &path, uint64_t entry_count)
           : nvm_data(path + "/cowbased_data_file", entry_count * sizeof(InplaceCow<entry_size>))
             , entry_count(entry_count)
   {
      assert(nvm_data.GetByteCount()>=entry_size * entry_count);

      memset(nvm_data.Data(), 'a', nvm_data.GetByteCount());
      entries = (InplaceCow<entry_size> *) nvm_data.Data();
      Random ranny;
      for (uint32_t i = 0; i<entry_count; i++) {
         entries[i].Init(ranny);
      }
      pmem_persist(nvm_data.Data(), nvm_data.GetByteCount());
   }

   void DoUpdate(const Operation<entry_size> &op)
   {
      assert(op.entry_id<entry_count);
      entries[op.entry_id].Write((const char *) &op);

      Operation<entry_size> asd;
      entries[op.entry_id].Read((char *) &asd);
   }

   uint64_t ReadSingleResult(Operation<entry_size> &result)
   {
      entries[result.entry_id].Read((char *) &result);
      return result.entry_id;
   }
};
// -------------------------------------------------------------------------------------
}
// -------------------------------------------------------------------------------------
#pragma once
// -------------------------------------------------------------------------------------
#include <type_traits>
#include <cassert>
// -------------------------------------------------------------------------------------
template<typename Value>
class OverridingList {
public:
   OverridingList()
           : head(nullptr) {}

   void Push(Value ptr)
   {
      Entry *entry = reinterpret_cast<Entry *>(ptr);
      entry->next = head;
      head = ptr;
   }

   Value Pop()
   {
      assert(!Empty());
      Value result = head;
      head = reinterpret_cast<Entry *>(head)->next;
      return result;
   }

   Value Top()
   {
      assert(!Empty());
      return head;
   }

   bool Empty() const
   {
      return head == nullptr;
   }

private:
   static_assert(std::is_pointer<Value>::value, "InPlaceList can not work on values.");

   struct Entry {
      Value next;
   };

   Value head;
};
// -------------------------------------------------------------------------------------

#include "OverridingList.hpp"
#include <experimental/coroutine>
#include <optional>
#include <iostream>
#include <cstring>
#include <vector>
#include <array>
#include <immintrin.h>
// -------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------
inline void Clwb(void *addr)
{
#ifdef NOCLWB
   (void) addr;
#else
   asm volatile(".byte 0x66; xsaveopt %0" : "+m" (*(volatile char *) addr));
#endif
}
// -------------------------------------------------------------------------------------
inline void SFence()
{
   _mm_sfence();
}
// -------------------------------------------------------------------------------------
template<class RESULT>
class Task {
public:
   /*
    * The promise is a proxy for the compiler generated code to deal with one specific coroutine.
    *
    * From https://lewissbaker.github.io/2017/11/17/understanding-operator-co-await:
    * The Promise interface specifies methods for customising the behaviour of the coroutine itself.
    * The library-writer is able to customise what happens when the coroutine is called, what happens
    * when the coroutine returns (either by normal means or via an unhandled exception) and customise
    * the behaviour of any co_await or co_yield expression within the coroutine.
    * */
   struct promise_type {
      static void *memory_buffer;
      static OverridingList<void *> free_list;
      static const uint64_t coroutine_count = 10; // Number of coroutines
      static const uint64_t coroutine_size = 88; // Just for debugging, in this case the size of each coroutine object should be the same

      static void Setup()
      {
         memory_buffer = malloc(coroutine_count * coroutine_size);
         for (uint32_t i = 0; i<coroutine_count; i++) {
            free_list.Push((char *) memory_buffer + (i * coroutine_size));
         }
      }

      static void *operator new(size_t n)
      {
         //         cout << "x alloc: " << n << endl;
         assert(memory_buffer && "Allocator not setup, use Setup().");
         assert(n<=coroutine_size && "Coroutine size is to large for buffer, increase coroutine_size.");
         return free_list.Pop();
      }

      static void operator delete(void *p) noexcept
      {
         //         cout << "x free " << endl;
         free_list.Push(p);
      }

      optional<RESULT> returned_value;

      Task get_return_object()
      {
         return experimental::coroutine_handle<promise_type>::from_promise(*this);
      }

      experimental::suspend_never initial_suspend()
      {
         return {};
      }

      experimental::suspend_always final_suspend()
      {
         return {};
      }

      void unhandled_exception()
      {
         // printf("asd\n"); enable to make object smaller ?!
         auto exceptionPtr = current_exception();
         if (exceptionPtr) {
            rethrow_exception(exceptionPtr);
         }
      }

      void return_value() {}

      void return_value(RESULT value)
      {
         returned_value.emplace(value);
      }

      RESULT yield_value(RESULT value)
      {
         returned_value.emplace(value);
      }
   };

   Task() {}

   Task(experimental::coroutine_handle <promise_type> handle)
           : coroutine_handle(handle) {}

   Task(Task &&o)
           : coroutine_handle(o.coroutine_handle)
   {
      o.coroutine_handle = nullptr;
   }

   Task &operator=(Task &&o)
   {
      coroutine_handle = o.coroutine_handle;
      o.coroutine_handle = nullptr;
      return *this;
   }

   Task(const Task &) = delete;

   Task &operator=(const Task &) = delete;

   ~Task()
   {
      if (coroutine_handle)
         coroutine_handle.destroy();
   }

   optional<RESULT> get_result()
   {
      return coroutine_handle.promise().returned_value;
   }

   experimental::coroutine_handle <promise_type> coroutine_handle;
};
// -------------------------------------------------------------------------------------
template<class T> OverridingList<void *> Task<T>::promise_type::free_list;
template<class T> void *Task<T>::promise_type::memory_buffer = nullptr;
// -------------------------------------------------------------------------------------
class Scheduler {
public:
   /*
    * The awaitable (and awaiter) is a way to handle the scheduling of multiple coroutines.
    *
    * From https://lewissbaker.github.io/2017/11/17/understanding-operator-co-await:
    * The Awaitable interface specifies methods that control the semantics of a co_await expression.
    * When a value is co_awaited, the code is translated into a series of calls to methods on the
    * awaitable object that allow it to specify: whether to suspend the current coroutine, execute
    * some logic after it has suspended to schedule the coroutine for later resumption, and execute
    * some logic after the coroutine resumes to produce the result of the co_await expression.
    * */
   struct awaiter {
      Scheduler &s;

      explicit awaiter(Scheduler &queue_scheduler)
              : s(queue_scheduler) {}

      // Check if we should really suspend the
      constexpr bool await_ready() const noexcept
      {
         return false;
      }

      // (After coroutine was suspended) Should be paused (true) or conintued (false) ?
      constexpr bool await_suspend(experimental::coroutine_handle<void> handle) const noexcept
      {
         return true;
      }

      // Called before coroutine is resumed. The return of this function is returned in the coroutine by co_await op
      constexpr int await_resume() const noexcept
      {
         return 1;
      }

      // there is actually a 'co_await' operator that would return the 'awaiter', but we dont need this indirection
   };

   awaiter schedule()
   {
      return awaiter(*this);
   }
};
// -------------------------------------------------------------------------------------
struct Node {
   static const uint32_t CAPACITY = 32;

   struct Entry {
      uint64_t key;
      uint64_t value;
   };

   uint32_t free_bits;
   uint8_t fingerprints[CAPACITY];

   array<uint8_t, 6> padding_and_soon_a_lock;

   Entry entries[CAPACITY];

   Node()
   {
      free_bits = ~0; // Everything is free
      memset(fingerprints, 0, CAPACITY * sizeof(uint8_t));
      padding_and_soon_a_lock = {0};
      memset(entries, 0, CAPACITY * sizeof(Entry));

      assert(((uint64_t) &entries[0].key) % 16 == 0); // Ensure that key and value are always on the same cache line
   }

   friend bool operator==(const Node &lhs, const Node &rhs)
   {
      if (lhs.free_bits != rhs.free_bits) {
         cout << "free_bits: " << lhs.free_bits << " " << rhs.free_bits << endl;
         return false;
      }
      for (uint32_t i = 0; i<CAPACITY; i++) {
         if (lhs.fingerprints[i] != rhs.fingerprints[i]) {
            cout << "fp" << endl;
            return false;
         }
         if (lhs.entries[i].key != rhs.entries[i].key) {
            cout << "key" << endl;
            return false;
         }
         if (lhs.entries[i].value != rhs.entries[i].value) {
            cout << "val" << endl;
            return false;
         }
      }
      return true;
   }
};
static_assert(sizeof(Node) == 560); // Just to check if I summed up correctly
// -------------------------------------------------------------------------------------
uint8_t FingerprintHash(uint64_t key)
{
   return key;
}
// -------------------------------------------------------------------------------------
uint32_t Node_GetFirstFreeSlot(Node *node)
{
   return _mm_tzcnt_32(node->free_bits);
}
// -------------------------------------------------------------------------------------
void Node_SetSlotAsUsed(Node *node, uint32_t slot)
{
   node->free_bits &= (~(1ull << slot));
}
// -------------------------------------------------------------------------------------
void Node_Insert_Normal(Node *node, uint64_t key, uint64_t value)
{
   uint32_t slot = Node_GetFirstFreeSlot(node);

   // Full ? -> reset
   if (slot == 32) {
      node->free_bits = 0;
      slot = 0;
   }

   // Add new entry and persist
   node->entries[slot].key = key;
   node->entries[slot].value = value;
   node->fingerprints[slot] = FingerprintHash(key);
   Clwb(&node->entries[slot].key);
   Clwb(&node->fingerprints[slot]);
   SFence();

   // Set new entry valid and persist
   Node_SetSlotAsUsed(node, slot);
   Clwb(&node->free_bits);
   SFence();
}
// -------------------------------------------------------------------------------------
Task<int> Node_Insert_Coro(Node *node, uint64_t key, uint64_t value, Scheduler &scheduler)
{
   uint32_t slot = Node_GetFirstFreeSlot(node);

   // Full ? -> reset
   if (slot == 32) {
      node->free_bits = 0;
      slot = 0;
   }

   // Add new entry and persist
   node->entries[slot].key = key;
   node->entries[slot].value = value;
   node->fingerprints[slot] = FingerprintHash(key);
   Clwb(&node->entries[slot].key);
   Clwb(&node->fingerprints[slot]);
   //@formatter:off
   co_await scheduler.schedule();
   //@formatter:on

   // Set new entry valid and persist
   Node_SetSlotAsUsed(node, slot);
   Clwb(&node->free_bits);
   //@formatter:off
   co_return 1;
   //@formatter:on
}
// -------------------------------------------------------------------------------------
uint64_t insert_count;
uint64_t node_count;
uint64_t group_size;
bool use_coroutines;
// -------------------------------------------------------------------------------------
void DoInsertsWithCoroutines(vector<Node> &nodes, const vector<uint32_t> &operations, uint32_t group_size)
{
   Scheduler scheduler;

   vector<Task<int>> groups;
   uint32_t i = 0;
   for (; i + group_size - 1<insert_count; i += group_size) {
      if (operations[i + 0] == operations[i + 1]) {
         cout << "collision in node: " << operations[i + 0] << endl;
      }

      // 1. task: persist key/value
      for (uint32_t g = 0; g<group_size; g++) {
         uint32_t node_id = operations[i + g];
         groups.emplace_back(Node_Insert_Coro(&nodes[node_id], i + g, i + g, scheduler));
      }
      SFence();

      // 2. task: set valid
      for (uint32_t g = 0; g<group_size; g++) {
         groups[g].coroutine_handle.resume();
         assert(groups[g].coroutine_handle.done());
      }
      groups.clear();
      SFence();
   }

   // Need to do some more inserts if insert_count%group_size != 0
   for (; i<insert_count; i++) {
      Node_Insert_Normal(&nodes[operations[i]], i, i);
   }
}
// -------------------------------------------------------------------------------------
void DoInsertsNormal(vector<Node> &nodes, const vector<uint32_t> &operations)
{
   for (uint32_t i = 0; i<insert_count; i++) {
      Node_Insert_Normal(&nodes[operations[i]], i, i);
   }
}
// -------------------------------------------------------------------------------------
template<class T>
void DoTimed(const T &foo)
{
   auto from = chrono::high_resolution_clock::now();
   foo();
   auto till = chrono::high_resolution_clock::now();
   uint64_t ms = chrono::duration_cast<chrono::milliseconds>(till - from).count();
   cout << "time: " << ms << "ms" << endl;
}
// -------------------------------------------------------------------------------------
void Validate(const vector<uint32_t> &operations)
{
#ifndef NDEBUG
   // Normal
   vector<Node> normal_nodes(node_count);
   DoInsertsNormal(normal_nodes, operations);

   // Do Coroutine
   vector<Node> coro_nodes(node_count);
   DoInsertsWithCoroutines(coro_nodes, operations, group_size);

   for (uint32_t i = 0; i<node_count; i++) {
      if (!(coro_nodes[i] == normal_nodes[i])) {
         cout << "ERROR: in node " << i << endl;
         exit(-1);
      }
   }
   assert(memcmp(&coro_nodes[0], &normal_nodes[0], node_count * sizeof(Node)) == 0);
   cout << "Validation: OK!" << endl;
#endif
}
// -------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
   Task<int>::promise_type::Setup();

   if (argc != 5) {
      cout << "usage: " << argv[0] << " node_count insert_count group_size [coro|normal]" << endl;
      throw;
   }

   node_count = atof(argv[1]);
   insert_count = atof(argv[2]);
   group_size = atof(argv[3]);
   use_coroutines = argv[4] == string("coro");

   // Config
   cout << "Config" << endl;
   cout << "------" << endl;
   cout << "node_count     " << node_count << endl;
   cout << "insert_count   " << insert_count << endl;
   cout << "group_size     " << group_size << endl;
   cout << "use_coroutines " << (use_coroutines ? "yes" : "no") << endl;
   cout << "------" << endl;

   if (group_size>insert_count || group_size == 0) {
      cout << "ERROR: Invalid group_size." << endl;
      return -1;
   }

   // Setup indexes for inserts
   vector<uint32_t> operations;
   operations.reserve(insert_count);
   uint64_t nodes_per_group = (node_count / group_size);
   for (uint64_t op = 0; op<insert_count; op++) {
      uint64_t group = op % group_size;
      operations.push_back(nodes_per_group * group + rand() % nodes_per_group);
   }

   // Perform experiment
   //   if (use_coroutines) {
   //      // Do Coroutine
   //      vector<Node> coro_nodes(node_count);
   //      DoInsertsWithCoroutines(coro_nodes, operations, group_size);
   //   } else {
   //      // Do Normal
   //      vector<Node> normal_nodes(node_count);
   //      DoInsertsNormal(normal_nodes, operations);
   //   }

   // Validate
   Validate(operations);

   return 0;
}
// -------------------------------------------------------------------------------------

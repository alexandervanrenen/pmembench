#include "OverridingList.hpp"
#include <experimental/coroutine>
#include <optional>
#include <iostream>
#include <cstring>
#include <vector>
#include <array>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <immintrin.h>
#include <xmmintrin.h>
// -------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------
struct Operation {
   uint32_t node_id;
   uint32_t key;
};
// -------------------------------------------------------------------------------------
// -------------------------------- Coro Boilerplate Code ------------------------------
// -------------------------------------------------------------------------------------
template<class RESULT>
class Task {
public:
   /*
    * The promise is a proxy for the compiler generated code to deal with one specific coroutine.
    *
    * From https://lewissbaker.github.io/2017/11/17/understanding-operator-co-await
    * The Promise interface specifies methods for customising the behaviour of the coroutine itself.
    * The library-writer is able to customise what happens when the coroutine is called, what happens
    * when the coroutine returns (either by normal means or via an unhandled exception) and customise
    * the behaviour of any co_await or co_yield expression within the coroutine.
    * */
   struct promise_type {
      static void *memory_buffer;
      static OverridingList<void *> free_list;
      static const uint64_t coroutine_count = 1000; // Number of coroutines
      static const uint64_t coroutine_size = 256; // Just for debugging, in this case the size of each coroutine object should be the same

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
         assert(n<=coroutine_size && "Coroutine size is too large for buffer, increase coroutine_size.");
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
    * From https://lewissbaker.github.io/2017/11/17/understanding-operator-co-await
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
uint64_t LOOKUP_COUNT;
uint64_t NODE_COUNT;
uint64_t GROUP_SIZE;
uint64_t RUN_COUNT = 1;
bool USE_RAM;
string PATH;
// -------------------------------------------------------------------------------------
// -------------------------------- Tree Code ------------------------------------------
// -------------------------------------------------------------------------------------
struct Node {
   static const uint32_t CAPACITY = 32;

   struct Entry {
      uint64_t key;
      uint64_t value;
   };

   uint32_t free_bits; // 4 byte (SUM = 4 byte)
   uint8_t fingerprints[CAPACITY]; // 32 byte (SUM = 36 byte)

   array<uint8_t, 28> padding_and_soon_a_lock; // 28 byte (SUM = 64 byte)

   Entry entries[CAPACITY]; // 512 byte (SUM = 576 byte = 9 cls)

   void Initialize()
   {
      free_bits = 0; // Everything is used
      padding_and_soon_a_lock = {0};

      for (uint32_t i = 0; i<CAPACITY; i++) {
         entries[i].key = i;
         entries[i].value = i;
         fingerprints[i] = i;
      }

      assert(((uint64_t) this) % 64 == 0); // Ensure that free_bits and fingerprints are on the same cache line
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
static_assert(sizeof(Node) == 576); // Just to check if I summed up correctly
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
bool Node_IsSlotUsed(Node *node, uint32_t slot)
{
   return (node->free_bits & (1ull << slot)) == 0;
}
// -------------------------------------------------------------------------------------
// -------------------------------- Normal Lookup --------------------------------------
// -------------------------------------------------------------------------------------
uint64_t Node_Lookup_Normal(Node *node, uint32_t key)
{
   uint8_t key_hash = FingerprintHash(key);
   for (uint32_t slot = 0; slot<Node::CAPACITY; slot++) {
      if (Node_IsSlotUsed(node, slot) && node->fingerprints[slot] == key_hash) {
         if (key == node->entries[slot].key) {
            return node->entries[slot].value;
         }
      }
   }

   assert(false); // No negative lookups
   return 0;
}
// -------------------------------------------------------------------------------------
uint64_t DoLookupNormal(Node *nodes, const vector<Operation> &operations)
{
   uint64_t sum = 0;
   for (uint32_t i = 0; i<LOOKUP_COUNT; i++) {
      sum += Node_Lookup_Normal(&nodes[operations[i].node_id], operations[i].key);
   }
   return sum;
}
// -------------------------------------------------------------------------------------
// -------------------------------- Coro Read ------------------------------------------
// -------------------------------------------------------------------------------------
Task<int> Node_LookupCoroRead(Node *node, uint32_t key, Scheduler &scheduler)
{
   // 1. Prefetch slots
   _mm_prefetch(&node->free_bits, _MM_HINT_T0);
   //@formatter:off
   co_await scheduler.schedule();
   //@formatter:on

   uint8_t key_hash = FingerprintHash(key);
   for (uint32_t slot = 0; slot<Node::CAPACITY; slot++) {
      if (Node_IsSlotUsed(node, slot) && node->fingerprints[slot] == key_hash) {

         // 2. Prefetch key/value
         _mm_prefetch(&node->entries[slot], _MM_HINT_T0);
         //@formatter:off
         co_await scheduler.schedule();
         //@formatter:on

         if (key == node->entries[slot].key) {
            //@formatter:off
            co_return node->entries[slot].value;
            //@formatter:on
         }
      }
   }

   assert(false); // No negative lookups
   //@formatter:off
   co_return 0;
   //@formatter:on
}
// -------------------------------------------------------------------------------------
uint64_t DoLookupCoroRead(Node *nodes, const vector<Operation> &operations)
{
   uint64_t sum = 0;
   Scheduler scheduler;

   vector<Task<int>> groups;
   uint32_t i = 0;
   for (; i + GROUP_SIZE - 1<LOOKUP_COUNT; i += GROUP_SIZE) {
      // 1. task: fetch node
      for (uint32_t g = 0; g<GROUP_SIZE; g++) {
         uint32_t node_id = operations[i + g].node_id;
         uint32_t key = operations[i + g].key;
         groups.emplace_back(Node_LookupCoroRead(&nodes[node_id], key, scheduler));
      }

      // 2. task: fetch key/value
      for (uint32_t g = 0; g<GROUP_SIZE; g++) {
         groups[g].coroutine_handle.resume();
      }

      // 3. task: set valid
      for (uint32_t g = 0; g<GROUP_SIZE; g++) {
         assert(!groups[g].coroutine_handle.done());
         groups[g].coroutine_handle.resume();
         assert(groups[g].coroutine_handle.done());
         sum += groups[g].get_result().value();
      }
      groups.clear();
   }

   // Need to do some more lookups if lookup_count%GROUP_SIZE != 0
   for (; i<LOOKUP_COUNT; i++) {
      sum += Node_Lookup_Normal(&nodes[operations[i].node_id], operations[i].key);
   }

   return sum;
}
// -------------------------------------------------------------------------------------
uint8_t *AllocateMemory(bool use_ram, uint64_t byte_count, uint32_t id)
{
   if (use_ram) {
      uint8_t *result = new uint8_t[byte_count + 64];
      while (((uint64_t) result) % 64 != 0) // Align to 64 byte
         result++;
      return result;
   } else {
      int fd = open((PATH + "/file_" + to_string(id)).c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
      int td = ftruncate(fd, byte_count);
      if (fd<0 || td<0) {
         cout << "unable to create file" << endl;
         exit(-1);
      }
      uint8_t *result = (uint8_t *) mmap(nullptr, byte_count, PROT_WRITE, MAP_SHARED, fd, 0);
      return result;
   }
}
// -------------------------------------------------------------------------------------
Node *CreateNodes(uint32_t id)
{
   uint8_t *mem = AllocateMemory(USE_RAM, NODE_COUNT * sizeof(Node), id);
   return (Node *) mem;
}
// -------------------------------------------------------------------------------------
template<class T>
void Validate(const vector<Operation> &operations, const T &foo)
{
#ifndef NDEBUG
   // Prepare nodes
   Node *nodes = CreateNodes(0);
   for (uint32_t i = 0; i<NODE_COUNT; i++) {
      nodes[i].Initialize();
   }

   // Do actual lookups
   uint64_t actual = foo(nodes, operations);

   // Calculate what the result shoould be
   uint64_t expected = 0;
   for (auto &op : operations) {
      expected += op.key;
   }

   // Validate
   if (expected != actual) {
      cout << "expected: " << expected << " vs actual: " << actual << endl;
      throw;
   }

   Node *nodes2 = CreateNodes(0);
   for (uint32_t i = 0; i<NODE_COUNT; i++) {
      nodes2[i].Initialize();
   }
   assert(memcmp(nodes, nodes2, NODE_COUNT * sizeof(Node)) == 0);
   cout << "Validation: OK!" << endl;
#endif
}
// -------------------------------------------------------------------------------------
template<class T>
void DoExperiment(Node *nodes, const vector<Operation> &operations, const string &coro_style, const T &foo)
{
   // Initialize nodes
   for (uint32_t i = 0; i<NODE_COUNT; i++) {
      nodes[i].Initialize();
   }

   uint64_t res = 0;
   auto from = chrono::high_resolution_clock::now();
   for (uint32_t run = 0; run<RUN_COUNT; run++) {
      res = foo(nodes, operations);
   }
   auto till = chrono::high_resolution_clock::now();
   uint64_t ns = chrono::duration_cast<chrono::nanoseconds>(till - from).count();
   uint64_t lookups_per_second = (LOOKUP_COUNT * RUN_COUNT * 1e9) / ns;

   //@formatter:off
   cout << "res: "
        << " node_count: " << NODE_COUNT
        << " lookup_count: " << LOOKUP_COUNT
        << " runs: " << RUN_COUNT
        << " group_size: " << GROUP_SIZE
        << " coro_style: " << coro_style
        << " use_ram: " << (USE_RAM ? "yes" : "no")
        << " byte_count(GB): " << (NODE_COUNT * sizeof(Node)) / 1000000 / 1000.0f
        << " path: " << PATH
        << " res: " << res
        << " lookups/s: " << lookups_per_second
        << endl;
   //@formatter:on
}
// -------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
   Task<int>::promise_type::Setup();

   if (argc != 6) {
      cout << "usage: " << argv[0] << " node_count lookup_count group_size [ram|nvm] path" << endl;
      throw;
   }

   NODE_COUNT = atof(argv[1]);
   LOOKUP_COUNT = atof(argv[2]);
   GROUP_SIZE = atof(argv[3]);
   USE_RAM = argv[4] == string("ram");
   PATH = argv[5];

   // Config
   cout << "Config" << endl;
   cout << "------" << endl;
   cout << "node_count     " << NODE_COUNT << endl;
   cout << "lookup_count   " << LOOKUP_COUNT << endl;
   cout << "group_size     " << GROUP_SIZE << endl;
   cout << "runs           " << RUN_COUNT << endl;
   cout << "use_ram        " << (USE_RAM ? "yes" : "no") << endl;
   cout << "path           " << PATH << endl;
   cout << "------" << endl;

   if (GROUP_SIZE>LOOKUP_COUNT || GROUP_SIZE == 0) {
      cout << "ERROR: Invalid group_size." << endl;
      return -1;
   }

   // Setup indexes for lookups
   vector<Operation> operations;
   operations.reserve(LOOKUP_COUNT);
   uint64_t nodes_per_group = (NODE_COUNT / GROUP_SIZE);
   for (uint64_t op = 0; op<LOOKUP_COUNT; op++) {
      uint32_t node_id = rand() % NODE_COUNT;
      uint32_t key = rand() % Node::CAPACITY;
      operations.push_back(Operation{node_id, key});
   }

   // For testing if it works !!
   Validate(operations, DoLookupNormal);
   Validate(operations, DoLookupCoroRead);

   // Perform experiment
   Node *nodes = CreateNodes(0);
   DoExperiment(nodes, operations, "scalar", DoLookupNormal);
   DoExperiment(nodes, operations, "coro_read", DoLookupCoroRead);

   return 0;
}
// -------------------------------------------------------------------------------------

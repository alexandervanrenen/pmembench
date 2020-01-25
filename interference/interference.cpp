#include "PageFlusher.hpp"
// -------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
   ub8 PAGE_COUNT_PER_THREAD = 625000;
   ub8 PAGE_FLUSH_THREADS = 1;
   string NVM_PATH = "";

   if (argc != 2) {
      cout << "usage: " << argv[0] << " path" << endl;
      throw;
   }

   NVM_PATH = argv[1];

   cerr << "Config:" << endl;
   cerr << "----------------------------" << endl;
   cerr << "PAGE_COUNT_PER_THREAD: " << PAGE_COUNT_PER_THREAD << endl;
   cerr << "NVM_PATH:              " << NVM_PATH << endl;
#ifdef STREAMING
   cerr << "STREAMING:             " << "yes" << endl;
#else
   cerr << "STREAMING:             " << "no" << endl;
#endif

   vector<unique_ptr<PageFlusher>> page_flushers;
   vector<unique_ptr<thread>> page_flusher_threads;

   for (ub4 i = 0; i<PAGE_FLUSH_THREADS; i++) {
      page_flushers.emplace_back(make_unique<PageFlusher>(NVM_PATH + "/page_flush_" + to_string(i), PAGE_COUNT_PER_THREAD));
      page_flusher_threads.emplace_back(make_unique<thread>([&, i]() {
         page_flushers[i]->Run(i);
      }));
   }

   for (ub4 i = 0; i<PAGE_FLUSH_THREADS; i++) {
      while (!page_flushers[i]->ready);
   }

   for (ub4 i = 0; i<PAGE_FLUSH_THREADS; i++) {
      page_flushers[i]->run = true;
   }

   while (1);
}
// -------------------------------------------------------------------------------------
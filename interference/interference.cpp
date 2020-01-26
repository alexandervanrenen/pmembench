#include "PageFlusher.hpp"
#include "LogWriter.hpp"
#include "SequentialReader.hpp"
// -------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------
// Page flush config
ub8 PAGE_COUNT_PER_THREAD = 625000;
ub8 PAGE_FLUSH_THREADS = 1;
// -------------------------------------------------------------------------------------
// Log writer config
ub8 LOG_BYTE_COUNT = 1e9;
ub8 LOG_WRITE_THREADS = 1;
// -------------------------------------------------------------------------------------
// Sequential reader config
ub8 SEQ_READER_BYTE_COUNT = 1e9;
ub8 SEQ_READER_THREADS = 1;
bool SEQ_READER_USE_RAM = true;
// -------------------------------------------------------------------------------------
// Common config
string NVM_PATH = "";
// -------------------------------------------------------------------------------------
void TestOutPageFlusher()
{
   vector<unique_ptr<PageFlusher>> page_flushers;
   vector<unique_ptr<thread>> threads;

   for (ub4 i = 0; i<PAGE_FLUSH_THREADS; i++) {
      page_flushers.emplace_back(make_unique<PageFlusher>(NVM_PATH + "/page_flush_" + to_string(i), PAGE_COUNT_PER_THREAD));
      threads.emplace_back(make_unique<thread>([&, i]() {
         page_flushers[i]->Run(i);
      }));
   }

   for (ub4 i = 0; i<PAGE_FLUSH_THREADS; i++) {
      while (!page_flushers[i]->ready);
   }

   for (ub4 i = 0; i<PAGE_FLUSH_THREADS; i++) {
      page_flushers[i]->run = true;
   }

   cout << "main thread endless spin .." << endl;
   while (1);
}
// -------------------------------------------------------------------------------------
void TestOutLogWriter()
{
   vector<unique_ptr<LogWriter>> log_writers;
   vector<unique_ptr<thread>> threads;

   for (ub4 i = 0; i<LOG_WRITE_THREADS; i++) {
      log_writers.emplace_back(make_unique<LogWriter>(NVM_PATH + "/log_writer_" + to_string(i), LOG_BYTE_COUNT));
      threads.emplace_back(make_unique<thread>([&, i]() {
         log_writers[i]->Run(i);
      }));
   }

   for (ub4 i = 0; i<LOG_WRITE_THREADS; i++) {
      while (!log_writers[i]->ready);
   }

   for (ub4 i = 0; i<LOG_WRITE_THREADS; i++) {
      log_writers[i]->run = true;
   }

   cout << "main thread endless spin .." << endl;
   while (1);
}
// -------------------------------------------------------------------------------------
void TestOutSeqReader()
{
   vector<unique_ptr<SequentialReader>> seq_readers;
   vector<unique_ptr<thread>> threads;

   for (ub4 i = 0; i<SEQ_READER_THREADS; i++) {
      seq_readers.emplace_back(make_unique<SequentialReader>(NVM_PATH + "/seq_reader_" + to_string(i), SEQ_READER_BYTE_COUNT, SEQ_READER_USE_RAM));
      threads.emplace_back(make_unique<thread>([&, i]() {
         seq_readers[i]->Run(i);
      }));
   }

   for (ub4 i = 0; i<LOG_WRITE_THREADS; i++) {
      while (!seq_readers[i]->ready);
   }

   for (ub4 i = 0; i<LOG_WRITE_THREADS; i++) {
      seq_readers[i]->run = true;
   }

   cout << "main thread endless spin .." << endl;
   while (1);
}
// -------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
   if (argc != 2) {
      cout << "usage: " << argv[0] << " path" << endl;
      throw;
   }

   NVM_PATH = argv[1];

   cerr << "Config:" << endl;
   cerr << "----------------------------" << endl;
   cerr << "PAGE_COUNT_PER_THREAD: " << PAGE_COUNT_PER_THREAD << endl;
   cerr << "PAGE_FLUSH_THREADS:    " << PAGE_FLUSH_THREADS << endl;
   cerr << "LOG_WRITE_THREADS:     " << LOG_WRITE_THREADS << endl;
   cerr << "LOG_BYTE_COUNT:        " << LOG_BYTE_COUNT << endl;

   cerr << "NVM_PATH:              " << NVM_PATH << endl;
#ifdef STREAMING
   cerr << "STREAMING:             " << "yes" << endl;
#else
   cerr << "STREAMING:             " << "no" << endl;
#endif

   TestOutSeqReader();
}
// -------------------------------------------------------------------------------------

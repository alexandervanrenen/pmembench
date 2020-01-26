#include "PageFlusher.hpp"
#include "LogWriter.hpp"
#include "SequentialReader.hpp"
#include "RandomReader.hpp"
// -------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------
ub8 PAGE_FLUSH_PAGE_COUNT = 625000;
ub8 LOG_BYTE_COUNT = 1e9;
ub8 SEQ_READER_BYTE_COUNT = 10e9;
ub8 RND_READER_BYTE_COUNT = 1e9;
// -------------------------------------------------------------------------------------
ub4 SEQ_RAM_THREADS = 0;
ub4 SEQ_NVM_THREADS = 0;
ub4 RND_RAM_THREADS = 0;
ub4 RND_NVM_THREADS = 0;
ub4 LOG_NVM_THREADS = 0;
ub4 PAGE_NVM_THREADS = 0;
// -------------------------------------------------------------------------------------
vector<unique_ptr<SequentialReader>> seq_ram_readers;
vector<unique_ptr<SequentialReader>> seq_nvm_readers;
vector<unique_ptr<RandomReader>> rnd_ram_readers;
vector<unique_ptr<RandomReader>> rnd_nvm_readers;
vector<unique_ptr<LogWriter>> log_writers;
vector<unique_ptr<PageFlusher>> page_flushers;
vector<Worker *> all_workers;
vector<unique_ptr<thread>> all_threads;
// -------------------------------------------------------------------------------------
// Common config
string NVM_PATH = "";
string CONFIG_STRING = "";
// -------------------------------------------------------------------------------------
void CreateAllThreads()
{
   // Sequential read ram
   for (ub4 tid = 0; tid<SEQ_RAM_THREADS; tid++) {
      seq_ram_readers.emplace_back(make_unique<SequentialReader>(NVM_PATH + "/seq_ram_reader_" + to_string(tid), SEQ_READER_BYTE_COUNT, true, tid, CONFIG_STRING));
      all_threads.emplace_back(make_unique<thread>([&, tid]() {
         seq_ram_readers[tid]->Run();
      }));
      all_workers.push_back(seq_ram_readers[tid].get());
   }

   // Sequential read nvm
   for (ub4 tid = 0; tid<SEQ_NVM_THREADS; tid++) {
      seq_nvm_readers.emplace_back(make_unique<SequentialReader>(NVM_PATH + "/seq_nvm_reader_" + to_string(tid), SEQ_READER_BYTE_COUNT, false, tid, CONFIG_STRING));
      all_threads.emplace_back(make_unique<thread>([&, tid]() {
         seq_nvm_readers[tid]->Run();
      }));
      all_workers.push_back(seq_nvm_readers[tid].get());
   }

   // Random read ram
   for (ub4 tid = 0; tid<RND_RAM_THREADS; tid++) {
      rnd_ram_readers.emplace_back(make_unique<RandomReader>(NVM_PATH + "/rnd_ram_reader_" + to_string(tid), RND_READER_BYTE_COUNT, true, tid, CONFIG_STRING));
      all_threads.emplace_back(make_unique<thread>([&, tid]() {
         rnd_ram_readers[tid]->Run();
      }));
      all_workers.push_back(rnd_ram_readers[tid].get());
   }

   // Random read nvm
   for (ub4 tid = 0; tid<RND_NVM_THREADS; tid++) {
      rnd_nvm_readers.emplace_back(make_unique<RandomReader>(NVM_PATH + "/rnd_nvm_reader_" + to_string(tid), RND_READER_BYTE_COUNT, false, tid, CONFIG_STRING));
      all_threads.emplace_back(make_unique<thread>([&, tid]() {
         rnd_nvm_readers[tid]->Run();
      }));
      all_workers.push_back(rnd_nvm_readers[tid].get());
   }

   // Log writer
   for (ub4 tid = 0; tid<LOG_NVM_THREADS; tid++) {
      log_writers.emplace_back(make_unique<LogWriter>(NVM_PATH + "/log_writer_" + to_string(tid), LOG_BYTE_COUNT, tid, CONFIG_STRING));
      all_threads.emplace_back(make_unique<thread>([&, tid]() {
         log_writers[tid]->Run();
      }));
      all_workers.push_back(log_writers[tid].get());
   }

   // Page flush
   for (ub4 tid = 0; tid<PAGE_NVM_THREADS; tid++) {
      page_flushers.emplace_back(make_unique<PageFlusher>(NVM_PATH + "/page_flush_" + to_string(tid), PAGE_FLUSH_PAGE_COUNT, tid, CONFIG_STRING));
      all_threads.emplace_back(make_unique<thread>([&, tid]() {
         page_flushers[tid]->Run();
      }));
      all_workers.push_back(page_flushers[tid].get());
   }
}
// -------------------------------------------------------------------------------------
void WaitForAllToGetReady()
{
   for (auto iter : all_workers) {
      while (!iter->ready) {
         usleep(1e6); // Check every second
      }
   }
}
// -------------------------------------------------------------------------------------
void StartAll()
{
   for (auto iter : all_workers) {
      iter->run = true;
   }
}
// -------------------------------------------------------------------------------------
void WaitForAllToPerformAtLeastInterations(ub4 iteration_count)
{
   for (auto iter : all_workers) {
      while (iter->performed_iteration_count<iteration_count) {
         usleep(1e6); // Check every second
      }
   }
}
// -------------------------------------------------------------------------------------
void StopAll()
{
   for (auto iter : all_workers) {
      iter->stop = true;
   }
}
// -------------------------------------------------------------------------------------
void WaitForAllToDie()
{
   for (auto &iter : all_threads) {
      iter->join();
   }
}
// -------------------------------------------------------------------------------------
void PrintAll(ub4 iteration)
{
   for (auto iter : all_workers) {
      iter->PrintResultOfLastIteration(iteration);
   }
}
// -------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
   if (argc != 8) {
      cout << "usage: " << argv[0] << " SEQ_RAM SEQ_NVM RND_RAM RND_NVM LOG_NVM PAGE_NVM path" << endl;
      throw;
   }

   SEQ_RAM_THREADS = stof(argv[1]);
   SEQ_NVM_THREADS = stof(argv[2]);
   RND_RAM_THREADS = stof(argv[3]);
   RND_NVM_THREADS = stof(argv[4]);
   LOG_NVM_THREADS = stof(argv[5]);
   PAGE_NVM_THREADS = stof(argv[6]);
   NVM_PATH = argv[7];


   //@formatter:off
   CONFIG_STRING = to_string(SEQ_RAM_THREADS) + "_"
                 + to_string(SEQ_NVM_THREADS) + "_"
                 + to_string(RND_RAM_THREADS) + "_"
                 + to_string(RND_NVM_THREADS) + "_"
                 + to_string(LOG_NVM_THREADS) + "_"
                 + to_string(PAGE_NVM_THREADS);
   //@formatter:on

   cout << "Config:" << endl;
   cout << "----------------------------" << endl;
   cout << "SEQ_READER_BYTE_COUNT: " << SEQ_READER_BYTE_COUNT << endl;
   cout << "RND_READER_BYTE_COUNT: " << RND_READER_BYTE_COUNT << endl;
   cout << "LOG_BYTE_COUNT:        " << LOG_BYTE_COUNT << endl;
   cout << "PAGE_FLUSH_PAGE_COUNT: " << PAGE_FLUSH_PAGE_COUNT << endl;
   cout << "SEQ_RAM_THREADS:       " << SEQ_RAM_THREADS << endl;
   cout << "SEQ_NVM_THREADS:       " << SEQ_NVM_THREADS << endl;
   cout << "RND_RAM_THREADS:       " << RND_RAM_THREADS << endl;
   cout << "RND_NVM_THREADS:       " << RND_NVM_THREADS << endl;
   cout << "LOG_NVM_THREADS:       " << LOG_NVM_THREADS << endl;
   cout << "PAGE_NVM_THREADS:      " << PAGE_NVM_THREADS << endl;
   cout << "CONFIG_STRING:         " << CONFIG_STRING << endl;

   cerr << "NVM_PATH:              " << NVM_PATH << endl;
#ifdef STREAMING
   cerr << "STREAMING:             " << "yes" << endl;
#else
   cerr << "STREAMING:             " << "no" << endl;
#endif

   CreateAllThreads();
   WaitForAllToGetReady();
   StartAll();
   WaitForAllToPerformAtLeastInterations(10); // First one might be bad if not every one has started and last on might be bad because some body might finish earlier
   StopAll();
   WaitForAllToDie();
   PrintAll(2);

   cout << "all good :)" << endl;
   return 0;
}
// -------------------------------------------------------------------------------------

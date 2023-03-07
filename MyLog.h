#ifndef MYLOG_H
#define MYLOG_H

#include <pthread.h>

#include <fstream>
#include <iostream>
using namespace std;



// Log output(thread safe)
class MyLog : private fstream {
 public:
  MyLog();
  ~MyLog();

 private:
  bool m_bIsInit;
  pthread_mutex_t m_mutex;

 private:
  void Output(const string & msg);

 public:
  bool Init(const string & path);                        // initialize
  void Print(const string & msg, int unq_id = 0);        // print information
  void PrintNote(const string & msg, int unq_id = 0);    // print NOTE information
  void PrintWaning(const string & msg, int unq_id = 0);  // print warning information
  void PrintError(const string & msg, int unq_id = 0);   // print Error information
};

#endif

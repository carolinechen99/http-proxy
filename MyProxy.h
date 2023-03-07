#ifndef MYPROXY_H
#define MYPROXY_H

#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/socket.h>

#include <fstream>
#include <iostream>
#include <thread>
#include <cstring>
#include <unistd.h>


#include "MyLog.h"
#include "MyHandle.h"
#include "cache.hpp"
#include "connectInfo.hpp"
#include "response.hpp"

using namespace std;

// proxy class
class MyProxy {
 public:
  MyProxy();
  ~MyProxy();

 private:
  MyHandle m_handle;

 public:
  MyLog m_log;
  Cache m_cache;

 public:
  bool Init();                   // initialize
  int Run(const string & port);  // proxy startup entry
  int CreateSocket(const char * host,
                   const string & port,
                   struct addrinfo ** res_ptr,
                   int * hSocket);  // creat socket
  MyHandle * GetHandle(){
    return &m_handle;
  }

 private:
  int SetBindSocket(struct addrinfo * res, int * hSocket);  // setup and bind socket
  int StartListen(int hSocket);                             // start listening
  ConnectInfo * AcceptSocket(int hSocket, int unq_id);      // accept socket

};

extern MyProxy proxy;

#endif

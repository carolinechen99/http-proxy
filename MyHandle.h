#ifndef MYHANDLE_H
#define MYHANDLE_H

#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>

#include <vector>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <mutex>
using namespace std;

#include "connectInfo.hpp"
// #include "MyProxy.h"
#include "cache.hpp"

// enum CacheState{ NO_CACHE, EXPIRING, REVALIDATION };
// const char * cache_state[3] = {"no-cache", "expires", "re-validation" };

typedef void * (*pThreadFunc)(void *);
class MyProxy;

// // connect information
// class ConnectInfo {
//  public:
//   ConnectInfo(int uid, int hSocket, char * ip);
//   ~ConnectInfo();

//  public:
//   int m_uid;
//   int m_client_socket;
//   string m_client_ip;

//   // after parsing the received message, the following information is obtained
//   int m_method;
//   string m_http_head;
//   string m_http_body;
//   string m_req_line;
//   string m_uri;
//   string m_host;
//   string m_host_port;
//   int m_server_socket;
//   bool m_bSvrSockOpen;
// }; 
class MyHandle {
 public:
  MyHandle();
  ~MyHandle();

 public:
  static void HandleFunc(ConnectInfo * info);  // process sub-thread



 private:
  static string GetCurTime();                                    // get current time
  // req_msg type change from string to vector<char>
  static int Parse(vector<char> & req_msg, ConnectInfo & info);  // parse request message
  static int GetHttpHead(vector<char> & req_msg, string & http_head);  // get http head
  static int GetHttpBody(ConnectInfo & info, vector<char>& req_msg); // get http body
  static int GetRequestLine(string & http_head, string & req_line);  // get request line
  static int GetHttpHeadParam(
      string & http_head,
      string name,
      string &
          value);  // get the parameter information in the http header, format "NAME: VALUE\r\n"
  static int ParseMethodWithURI(string & req_line,
                                string & uri);  // parse the method and uri
  static int ParseHostPort(string & http_head,
                           string & host,
                           string & port);       // parse server host:port
  static int ConnectServer(ConnectInfo & info);  // connect the server

  static void HandleConnect(ConnectInfo & info, vector<char> & req_msg);  // handle connect

  static void HandleGet(ConnectInfo & info, std::vector<char> & req_msg, Cache &cache);  // handle get

  static void HandlePost(ConnectInfo & info, vector<char> & req_msg);  // handle post



  // // thread function
  // void * ThreadFunc(void * arg){
  //   ConnectInfo * info = (ConnectInfo *)arg;
  //   HandleFunc(info);
  //   delete info;
  //   return NULL;
  // }

  // receive data from web server
  int RecvFromServer(Response &response, vector<char> & recv_msg, ConnectInfo &info);

  // revalidate the cache and write to log file
  int RevaliGetResponse(Cache &cache, ConnectInfo &info, vector<char> &req_msg);

  bool paraExist(string& head, string key, string value);
  bool needRevalidation(Response &old_response, Response &new_response);
};



#endif

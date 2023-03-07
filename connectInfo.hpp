#ifndef CONNECTINFO_H
#define CONNECTINFO_H

#include <string>
using namespace std;


class ConnectInfo {
 public:
  ConnectInfo(int uid, int hSocket, char * ip);
  ~ConnectInfo();

 public:
  int m_uid;
  int m_client_socket;
  string m_client_ip;

  // after parsing the received message, the following information is obtained
  int m_method;
  string m_http_head;
  string m_http_body;
  string m_req_line;
  string m_uri;
  string m_host;
  string m_host_port;
  int m_server_socket;
  bool m_bSvrSockOpen;

  // for GET Request
  bool req_chunked; // Transfer-Encoding: chunked
  bool req_gzip; // Accept-Encoding: gzip
  bool keep_alive; // Connection: keep-alive/close
  int content_length; // Content-Length: xxx
  string m_cache_control;
  string last_modified;
  string expires;


}; 

#endif
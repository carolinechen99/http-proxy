#include <ctime>
#include <sstream>
#include <regex>
#include <chrono>
#include <fstream>


#include "MyProxy.h"
// #include "MyHandle.h"


enum { TYPE_GET, TYPE_POST, TYPE_CONNECT, TYPE_UNKNOWN };
const char * req_type[4] = {"GET", "POST", "CONNECT", "UNKNOWN"};


ConnectInfo::ConnectInfo(int uid, int hSocket, char * ip) {
  m_uid = uid;
  m_client_socket = hSocket;
  m_server_socket = 0;
  m_client_ip = ip;
  m_method = TYPE_UNKNOWN;
  m_bSvrSockOpen = false;
  m_http_head = "";
  m_http_body = "";

  req_chunked = false;
  req_gzip = false;
  keep_alive = false;
  content_length = 0;
}

ConnectInfo::~ConnectInfo() {
  shutdown(m_client_socket, SHUT_RDWR);
  if (m_bSvrSockOpen)
    shutdown(m_server_socket, SHUT_RDWR);
}

MyHandle::MyHandle() {
}

MyHandle::~MyHandle() {
}

// Receive data from server
int MyHandle::RecvFromServer(Response &response, vector<char> & recv_msg, ConnectInfo &info) {
    char recv_buffer[65535];  // ipv4 total length

    int bytes_recv = recv(info.m_server_socket, recv_buffer, sizeof(recv_buffer), MSG_NOSIGNAL);
    if (bytes_recv <= 0)
      return -1;
    recv_msg.insert(recv_msg.end(), recv_buffer, recv_buffer + bytes_recv);
    response.recv_msg = recv_msg;
    return 0;
}




void MyHandle::HandleFunc(ConnectInfo * info) {
  vector<char> req_info;
  // // receive request message from client
  char recv_buffer[65535];  // ipv4 total length

    int bytes_recv = recv(info->m_client_socket, recv_buffer, sizeof(recv_buffer), MSG_NOSIGNAL);
    if (bytes_recv <= 0)
        goto HANDLE_END;
    req_info.insert(req_info.end(), recv_buffer, recv_buffer + bytes_recv);

  ////!!multi-thread!!////





  // parse
  if (Parse(req_info, *info) == -1)
    goto HANDLE_END;

  // process the parsed request
  switch (info->m_method) {
    case TYPE_CONNECT:
      HandleConnect(*info, req_info);
      break;
    case TYPE_GET:
      HandleGet(*info, req_info, proxy.m_cache);
      break;
    case TYPE_POST:
      HandlePost(*info, req_info);
      break;
    default:
      proxy.m_log.PrintError("Unknown request!");
      break;
  }

HANDLE_END:
  delete info;
}

string MyHandle::GetCurTime() {
  time_t now = time(0);
  return ctime(&now);
}

// req_msg type changed from string to vector<char>
int MyHandle::Parse(vector<char> & req_msg, ConnectInfo & info) {
  // get http head/request line
  if (GetHttpHead(req_msg, info.m_http_head) == -1 ||
      GetRequestLine(info.m_http_head, info.m_req_line) == -1)
    return -1;


  // parse line and uri
  info.m_method = ParseMethodWithURI(info.m_req_line, info.m_uri);
  if (info.m_method == TYPE_UNKNOWN)
    return -1;

  // POST - get http body
  if (info.m_method == TYPE_POST && GetHttpBody(info, req_msg))
    return -1;

  // parse server host:port
  if (ParseHostPort(info.m_http_head, info.m_host, info.m_host_port) == -1)
    return -1;
  
  // get cache-control
  string cache_control;
  proxy.GetHandle()->GetHttpHeadParam(info.m_http_head, "Cache-Control", cache_control);
  info.m_cache_control = cache_control;

  // (DEBUG)
  //if (info.m_uri != "http://www.httpbin.org/")
  //    return -1;

  // GET - get http body and other info
  if (info.m_method == TYPE_GET){
      std::string header;
      proxy.GetHandle()->GetHttpHead(req_msg, header);
      std::stringstream header_stream;
      header_stream.str(header);
      std::string line;
      while (std::getline(header_stream, line)) {
          // Check for chunked encoding
          if (line.find("Transfer-Encoding: chunked") != std::string::npos || line.find("Content-Length") != std::string::npos) {
              info.req_chunked = true;
          }
          // Check for gzip encoding
          if (line.find("Accept-Encoding: gzip") != std::string::npos) {
              info.req_gzip = true;
              
          }
          // Check for keep-alive connection
          if (line.find("Connection: keep-alive") != std::string::npos) {
              info.keep_alive = true;
             
          }
          // assign currernt time to info.last_modified(which is a string)
          info.last_modified = GetCurTime();

          //get expires if exists
          if (line.find("Expires: ") != std::string::npos) {
              info.expires = line.substr(9);
              
          }
      }
  }




  // print message
  proxy.m_log.Print(
      "\"" + info.m_req_line + "\" from " + info.m_client_ip + " @ " + GetCurTime(),
      info.m_uid);
  proxy.m_log.PrintNote("Host: " + info.m_host, info.m_uid);
  proxy.m_log.PrintNote("Port: " + info.m_host_port, info.m_uid);

  return 0;
}

// req_msg type changed from string to vector<char>
int MyHandle::GetHttpHead(vector<char> & req_msg, string & http_head) {
  size_t head_size;

  head_size = search(req_msg.begin(), req_msg.end(), std::vector<char>{'\r', '\n', '\r', '\n'}.begin(), std::vector<char>{'\r', '\n', '\r', '\n'}.end()) - req_msg.begin();
  if (head_size == string::npos)
    return -1;
  http_head = string(req_msg.begin(), req_msg.begin() + head_size);
  return 0;
}

// 
int MyHandle::GetHttpBody(ConnectInfo & info, vector<char>& req_msg) {
  //change req_msg type to vector<char>
  string len;
  if (GetHttpHeadParam(info.m_http_head, "Content-Length", len) == -1) {
    proxy.m_log.PrintError("POST: fail to get http body");
    return -1;
  }
  info.m_http_body = string(req_msg.begin() + info.m_http_head.length() + 4, req_msg.begin() + info.m_http_head.length() + 4 + atoi(len.c_str()));
  return 0;
}

// int MyHandle::GetHttpBody(ConnectInfo & info, vector<char>& req_msg) {
//   // If Content-Length header is present, get content length from HTTP header
//   string len;
//   if (GetHttpHeadParam(info.m_http_head, "Content-Length", len) != -1) {
//     int content_length = atoi(len.c_str());

//     // Determine how much data has already been received
//     int bytes_received = req_msg.size() - info.m_http_head.length() - 4;

//     // Keep receiving data until full HTTP message is received
//     while (bytes_received < content_length) {
//       char buf[65535];
//       int bytes_read = recv(info.m_client_socket, buf, sizeof(buf), 0);
//       if (bytes_read == -1) {
//         proxy.m_log.PrintError("POST: Error receiving data from client");
//         return -1;
//       }
//       bytes_received += bytes_read;
//       req_msg.insert(req_msg.end(), buf, buf + bytes_read);
//     }

//     // Extract HTTP body from full message
//     info.m_http_body = string(req_msg.begin() + info.m_http_head.length() + 4, req_msg.end());

//   } else {
//     // If Content-Length header is not present, assume message is chunked and receive chunks until end of message
//     string chunk_length_str;
//     int chunk_length = 0;

//     // Read chunk length
//     int pos = req_msg.size();
//     while (pos < (int)req_msg.size() + 64) {
//       char c;
//       int bytes_read = recv(info.m_client_socket, &c, 1, 0);
//       if (bytes_read != 1) {
//         proxy.m_log.PrintError("POST: Error receiving chunk length from client");
//         return -1;
//       }
//       if (c == '\r') {
//         break;
//       }
//       chunk_length_str += c;
//       pos++;
//     }
//     if (chunk_length_str.empty()) {
//       // If empty chunk length is received, exit loop
//       return 0;
//     }
//     chunk_length = stoi(chunk_length_str, 0, 16);

//     // If end of message is reached, exit loop
//     if (chunk_length == 0) {
//       return 0;
//     }

//     // Receive chunk and append to HTTP body
//     while (req_msg.size() < pos + chunk_length + 2) {
//       char buf[65535];
//       int bytes_read = recv(info.m_client_socket, buf, sizeof(buf), 0);
//       if (bytes_read == -1) {
//         proxy.m_log.PrintError("POST: Error receiving chunk from client");
//         return -1;
//       }
//       req_msg.insert(req_msg.end(), buf, buf + bytes_read);
//     }
//     info.m_http_body += string(req_msg.begin() + pos, req_msg.begin() + pos + chunk_length);
//     req_msg.erase(req_msg.begin() + pos, req_msg.begin() + pos + chunk_length + 2);

//     // Recursively call GetHttpBody until end of message is reached
//     return GetHttpBody(info, req_msg);
//   }

//   return 0;
// }








// req_msg type changed from string to vector<char>
int MyHandle::GetRequestLine(string & http_head, string & req_line) {
  size_t end_pos;

  end_pos = http_head.find("\r\n");
  if (end_pos == string::npos)
    return -1;
  req_line = http_head.substr(0, end_pos);
  return 0;
}

int MyHandle::GetHttpHeadParam(string & http_head, string name, string & value) {
  size_t param_start;
  size_t param_end;

  transform(http_head.begin(), http_head.end(), http_head.begin(), ::toupper);
  transform(name.begin(), name.end(), name.begin(), ::toupper);

  param_start = http_head.find(name + ": ");
  if (param_start == string::npos)
    return -1;
  param_start += (name.length() + 2);
  param_end = http_head.find("\r\n", param_start);
  if (param_end == string::npos)
    return -1;

  value = http_head.substr(param_start, param_end - param_start);
  return 0;
}

int MyHandle::ParseMethodWithURI(string & req_line, string & uri) {
  size_t method_size;
  size_t uri_size;
  string method;
  int method_type;

  // get method
  method_size = req_line.find(" ");
  if (method_size == string::npos)
    return TYPE_UNKNOWN;
  method = req_line.substr(0, method_size);
  if (strcasecmp(method.c_str(), "CONNECT") == 0)
    method_type = TYPE_CONNECT;
  else if (strcasecmp(method.c_str(), "GET") == 0)
    method_type = TYPE_GET;
  else if (strcasecmp(method.c_str(), "POST") == 0)
    method_type = TYPE_POST;
  else
    return TYPE_UNKNOWN;

  // get URI
  uri_size = req_line.find(" ", method_size + 1);
  if (uri_size == string::npos)
    return TYPE_UNKNOWN;
  uri_size = uri_size - (method_size + 1);
  uri = req_line.substr(method_size + 1, uri_size);
  return method_type;
}

int MyHandle::ParseHostPort(string & http_head, string & host, string & port) {
  string host_info;
  size_t split_pos;

  if (GetHttpHeadParam(http_head, "Host", host_info) == -1)
    return -1;

  split_pos = host_info.find(":");
  if (split_pos == string::npos) {
    host = host_info;
    port = "80";  // http default port number
  }
  else {
    host = host_info.substr(0, split_pos);
    port = host_info.substr(split_pos + 1);
  }

  return 0;
}

int MyHandle::ConnectServer(ConnectInfo & info) {
  struct addrinfo * res;

  if (proxy.CreateSocket(
          info.m_host.c_str(), info.m_host_port, &res, &info.m_server_socket) == -1)
    return -1;
  info.m_bSvrSockOpen = true;
  proxy.m_log.PrintNote("Connecting to "  + info.m_host + " on port "  + info.m_host_port + "...");
  if (connect(info.m_server_socket, res->ai_addr, res->ai_addrlen)) {
    proxy.m_log.PrintError("Fail to connect to server!", info.m_uid);
    freeaddrinfo(res);
    return -1;
  }
  freeaddrinfo(res);
  return 0;
}

 void MyHandle::HandlePost(ConnectInfo& info, vector<char>& req_msg) {
   char recv_buffer[65535];  // ipv4
   ssize_t recv_len;

   // establish TCP connect with server
   if (ConnectServer(info) == -1)
     return;

   if (send(info.m_server_socket, req_msg.data(), req_msg.size(), MSG_NOSIGNAL) <= 0)
     return;

   recv_len = recv(info.m_server_socket, recv_buffer, sizeof(recv_buffer), MSG_NOSIGNAL);
   if (recv_len <= 0)
     return;

   if (send(info.m_client_socket, recv_buffer, recv_len, MSG_NOSIGNAL) <= 0)
     return;
 }

void MyHandle::HandleConnect(ConnectInfo & info, vector<char> & req_msg) {
  char recv_buffer[65535];  // ipv4
  ssize_t recv_len;
  string msg;
  fd_set fds;
  int hSocket[2];
  int i;

  // tcp connection
  if (ConnectServer(info) == -1)
    return;
  
  proxy.m_log.Print("Responding \"HTTP/1.1 200 established\"", info.m_uid);

  // SUCCESS:return 200-established
  msg = "HTTP/1.1 200 established\r\n\r\n";
  if (send(info.m_client_socket, msg.c_str(), msg.length(), MSG_NOSIGNAL) == -1) {
    proxy.m_log.PrintError("Fail to send to client!", info.m_uid);
    return;
  }

  // http tunnel
  hSocket[0] = info.m_client_socket;
  hSocket[1] = info.m_server_socket;
  while (true) {
    FD_ZERO(&fds);
    FD_SET(hSocket[0], &fds);
    FD_SET(hSocket[1], &fds);
    if (select(max(hSocket[0], hSocket[1]) + 1, &fds, NULL, NULL, NULL) == -1)
      goto TUNNEL_CLOSE;

    for (i = 0; i < 2; i++) {
      if (FD_ISSET(hSocket[i], &fds)) {
        recv_len = recv(hSocket[i], recv_buffer, sizeof(recv_buffer), MSG_NOSIGNAL);
        if (recv_len <= 0)
          goto TUNNEL_CLOSE;
        if (send(hSocket[1 - i], recv_buffer, recv_len, MSG_NOSIGNAL) <= 0)
          goto TUNNEL_CLOSE;
      }
    }
  }
TUNNEL_CLOSE:
  proxy.m_log.Print("Tunnel closed", info.m_uid);
}

bool MyHandle::paraExist(string& head, string key, string value = "") {
    // Search for key in the header
    size_t pos = head.find(key);
    if (pos == string::npos) {
        // Key not found
        return false;
    }
    
    // If value is empty, return true if key is found
    if (value.empty()) {
        return true;
    }
    
    // Search for value after key
    size_t start_pos = pos + key.length();
    size_t end_pos = head.find("\r\n", start_pos);
    if (end_pos == string::npos) {
        // End of header not found
        return false;
    }
    
    string val = head.substr(start_pos, end_pos - start_pos);
    
    // Check if value matches
    return val == value;
}



/*If the request is a GET request, your proxy should check its cache, and print one of the following:
ID: not in cache
ID: in cache, but expired at EXPIREDTIME ID: in cache, requires validation
ID: in cache, valid
*/
// implement GET with std::lock_guard<std::mutex> lock(m_mutex);
void MyHandle::HandleGet(ConnectInfo & info, vector<char> & req_msg, Cache &cache) {
  //ID: "REQUEST" from IPFROM @ TIME
  proxy.m_log.Print(
    "\"" + info.m_req_line + "\" from " + info.m_client_ip + " @ " + GetCurTime(),
    info.m_uid);

  // check if the request is in cache
  Response get_response = cache.getResponse(info.m_req_line);
  // if get_response is emtpy, not in cache
  if (get_response.getStatusLine().empty() || get_response.getIsCacheable() == false) {
    proxy.m_log.Print("not in cache", info.m_uid);
  // connect to server
  ConnectServer(info);
  
  // send request to server
  proxy.m_log.Print("Requesting \"" + info.m_req_line + "\" from " + info.m_host, info.m_uid);
  if (send(info.m_server_socket, req_msg.data(), req_msg.size(), MSG_NOSIGNAL) <= 0){
    proxy.m_log.PrintError("Fail to send request to server!", info.m_uid);
    // return 502, and handle the error
    string msg = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
    if (send(info.m_client_socket, msg.c_str(), msg.length(), MSG_NOSIGNAL) == -1){
      proxy.m_log.PrintError("Fail inform client having problem sending to server!", info.m_uid);
      return;
    }
    return;
  }


  // receive response from server
  vector<char> recv_msg;
  //initilize the myhandle
  MyHandle *myhandle = proxy.GetHandle();
  if (myhandle->RecvFromServer(get_response, recv_msg, info)== -1 ){
    // return 502, and handle the error
    string msg = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
    if (send(info.m_client_socket, msg.c_str(), msg.length(), MSG_NOSIGNAL) == -1){
      proxy.m_log.PrintError("Fail to inform client having problem receiving from server!", info.m_uid);
      return;
    }
    proxy.m_log.PrintError("Fail to receive response from server", info.m_uid);
    return;
  }

  // parse response
  if (get_response.parseResponse(recv_msg, get_response, info) == -1) {
    // return 502, and handle the error
    // send 502 to client
    string msg = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
    if (send(info.m_client_socket, msg.c_str(), msg.length(), MSG_NOSIGNAL) == -1){
      proxy.m_log.PrintError("Fail to inform client having problem parsing response from server!", info.m_uid);
      return;
    }
    return;
  }
  //print value from get_response.parseResponse(recv_msg, get_response, info)
  proxy.m_log.Print("Status: " + get_response.getStatus(), info.m_uid);


  //ID: Received "RESPONSE" from SERVER
  proxy.m_log.Print("Received " + get_response.getStatusLine() + " from " + info.m_host, info.m_uid);

    if (get_response.getStatus() == "200") {
    //Check if cacheable, if yes, add to cache
    if (get_response.getIsCacheable()) {
      string currTime = proxy.GetHandle()->GetCurTime();
      cache.setResponse(info.m_req_line, get_response, currTime);
    }
    else{
      //if find "no-store", print not cacheable because "no-store"
      if (proxy.GetHandle()->paraExist(get_response.head, "Cache-Control: ", "no-store")) {
        proxy.m_log.Print("not cacheable because \"no-store\"", info.m_uid);
      }
      //if find "privte", print not cacheable because "private"
      else if (proxy.GetHandle()->paraExist(get_response.head, "Cache-Control: ", "private")) {
        proxy.m_log.Print("not cacheable because \"private\"", info.m_uid);
      }
    }
    // send response to client
    //ID: Responding "RESPONSE"
    proxy.m_log.Print("Responding \"" + get_response.getStatusLine() + "\"", info.m_uid);
    if (send(info.m_client_socket, recv_msg.data(), recv_msg.size(), MSG_NOSIGNAL) <= 0){
      proxy.m_log.PrintError("Fail to send response to client", info.m_uid);
      return;
    }
  }
}



// if it is in cache
else {
  // if status is 200
  if (get_response.getStatus() == "200"){
      // ID: cached, but requires re-validation$$
    if (info.m_cache_control.find("must-revalidate") != string::npos) {
        proxy.m_log.Print("in cache, requires validation", info.m_uid);
        //revalidate
        proxy.GetHandle()->RevaliGetResponse(cache, info, req_msg);
    }
    else if (info.m_cache_control.find("proxy-revalidate") != string::npos) {
        proxy.m_log.Print("in cache, requires validation", info.m_uid);
        proxy.GetHandle()->RevaliGetResponse(cache, info, req_msg);
    }
    else if (info.m_cache_control.find("max-age") != string::npos) {
        // get max-age
        string max_age;
        size_t pos = info.m_cache_control.find("max-age");
        if (pos != std::string::npos) {
            // if max-age is the last field
            if (info.m_cache_control.find(",") == string::npos) {
                max_age = info.m_cache_control.substr(pos + 7);
            }
            else {
                max_age = info.m_cache_control.substr(pos + 7, info.m_cache_control.find(",") - pos - 7);
            }
        }
        // there is no last_modified field, but max-age is 0
        if (max_age == "0") {
            proxy.m_log.Print("in cache, requires validation", info.m_uid);
            proxy.GetHandle()->RevaliGetResponse(cache, info, req_msg);
        }
        
        // if last_modified is not empty
        // If now - last_modified > max_age, need revalidate
        if (!info.last_modified.empty()) {
            time_t now = time(0);
            struct tm tm;
            strptime(info.last_modified.c_str(), "%a, %d %b %Y %H:%M:%S %Z", &tm);
            time_t last_modified_time = mktime(&tm);
            //calculate expire_time
            time_t expires_time = last_modified_time + stoi(max_age);
            struct tm *expires_tm = gmtime(&expires_time);
            char expire_time[100];
            strftime(expire_time, 100, "%a, %d %b %Y %H:%M:%S %Z", expires_tm);

            if (difftime(now, last_modified_time) > stoi(max_age)) {
              //check if max-stale is in info.m_cache_control
              if (info.m_cache_control.find("max-stale") != string::npos) {
                //get max-stale
                string max_stale;
                size_t pos = info.m_cache_control.find("max-stale");
                if (pos != std::string::npos) {
                  // if max-stale is the last field
                  if (info.m_cache_control.find(",") == string::npos) {
                    max_stale = info.m_cache_control.substr(pos + 9);
                  }
                  else {
                    max_stale = info.m_cache_control.substr(pos + 9, info.m_cache_control.find(",") - pos - 9);
                  }
                }
                //if max-stale is empty, then use the default value
                if (max_stale.empty()) {
                  proxy.m_log.Print("in cache, but expired at " + string(expire_time), info.m_uid);
                  proxy.GetHandle()->RevaliGetResponse(cache, info, req_msg);
                }
                //if max-stale is not empty, then check if now - last_modified > max_age + max_stale
                else {
                  if (difftime(now, last_modified_time) > stoi(max_age) + stoi(max_stale)) {
                    //update expire_time
                    expires_time = last_modified_time + stoi(max_age) + stoi(max_stale);
                    proxy.m_log.Print("in cache, but expired at " + string(expire_time), info.m_uid);
                    proxy.GetHandle()->RevaliGetResponse(cache, info, req_msg);
                  }
                  else {
                    proxy.m_log.Print("in cache, valid", info.m_uid);
                    //send response to client
                    //ID: Responding "RESPONSE"
                    proxy.m_log.Print("Responding \"" + get_response.getStatusLine() + "\"", info.m_uid);
                    if (send(info.m_client_socket, get_response.recv_msg.data(), get_response.recv_msg.size(), MSG_NOSIGNAL) <= 0){
                      proxy.m_log.PrintError("Fail to send response to client", info.m_uid);
                      return;
                    }
                  }
                }
            }
            //ID: cached, expired at$EXPIRES$$
            else {
                proxy.m_log.Print("in cache, but expired at " + string(expire_time), info.m_uid);
                // revalidate
                proxy.GetHandle()->RevaliGetResponse(cache, info, req_msg);

            } 
        }
        else{
          proxy.m_log.Print("in cache, valid", info.m_uid);
          //send response to client
          //ID: Responding "RESPONSE"
          proxy.m_log.Print("Responding \"" + get_response.getStatusLine() + "\"", info.m_uid);
          if (send(info.m_client_socket, get_response.recv_msg.data(), get_response.recv_msg.size(), MSG_NOSIGNAL) <= 0){
            proxy.m_log.PrintError("Fail to send response to client", info.m_uid);
            return;
          }
        }

    }
  }
      // if expires is not empty
    // if now > expires, need revalidate
    else if (!info.expires.empty()) {
        time_t now = time(0);
        struct tm tm;
        strptime(info.expires.c_str(), "%a, %d %b %Y %H:%M:%S %Z", &tm);
        time_t expires_time = mktime(&tm);
        if (difftime(now, expires_time) >= 0) {
            proxy.m_log.Print("in cache, but expired at " + info.expires, info.m_uid);
            proxy.GetHandle()->RevaliGetResponse(cache, info, req_msg);
        }
        //ID: cached, expires at$EXPIRES$$
        else {
            proxy.m_log.Print("in cache, valid", info.m_uid);
            //send response to client
            //ID: Responding "RESPONSE"
            proxy.m_log.Print("Responding \"" + get_response.getStatusLine() + "\"", info.m_uid);
            if (send(info.m_client_socket, get_response.recv_msg.data(), get_response.recv_msg.size(), MSG_NOSIGNAL) <= 0){
              proxy.m_log.PrintError("Fail to send response to client", info.m_uid);
              return;
            }
        }
    }
          // if there is no field, need revalidate
    else {
        proxy.m_log.Print("in cache, requires validation", info.m_uid);
        // send revalidate request
        proxy.GetHandle()->RevaliGetResponse(cache, info, req_msg);
        }
}
// if status is "304"
else if (get_response.status == "304") {
    //ID: Responding "RESPONSE"
    proxy.m_log.Print("Responding \"" + get_response.getStatusLine() + "\"", info.m_uid);
    // send response to client
    if (send(info.m_client_socket, get_response.recv_msg.data(), get_response.recv_msg.size(), MSG_NOSIGNAL) <= 0){
      proxy.m_log.PrintError("Fail to send response to client", info.m_uid);
      return;
    }
}
else {
  //malformed response and inform client
  proxy.m_log.PrintError("Malformed response", info.m_uid);
  string msg = "HTTP/1.1 502 Bad Gateway";
  if (send(info.m_client_socket, msg.c_str(), msg.length(), MSG_NOSIGNAL) == -1){
    proxy.m_log.PrintError("malformed!", info.m_uid);
    return;
  }
}

}


}







int MyHandle::RevaliGetResponse(Cache &cache, ConnectInfo &info, vector<char> &req_msg){
  // get response from cache
  Response old_response = cache.getResponse(info.m_req_line);

  // connect to server
  if (ConnectServer(info) == -1){
    proxy.m_log.PrintError("502 Bad Gateway", info.m_uid);
    // send 502 to client
    string msg = "HTTP/1.1 502 Bad Gateway";
    if (send(info.m_client_socket, msg.c_str(), msg.length(), MSG_NOSIGNAL) == -1){
      proxy.m_log.PrintError("Fail to inform client having problem connecting to server!", info.m_uid);
      return -1;
    }
    return -1;
  }

  // send request to server
  if (send(info.m_server_socket, req_msg.data(), req_msg.size(), MSG_NOSIGNAL) <= 0){
    proxy.m_log.PrintError("Fail to send request to server!", info.m_uid);
    // return 502, and handle the error
    string msg = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
    if (send(info.m_client_socket, msg.c_str(), msg.length(), MSG_NOSIGNAL) == -1){
      proxy.m_log.PrintError("Fail to inform client having problem sending to server!", info.m_uid);
      return -1;
    }
   return -1;
  }
  proxy.m_log.Print("Requesting " + info.m_req_line + " from " + info.m_host, info.m_uid);


  vector <char> recv_msg;
  Response new_response;
  MyHandle *myhandle = proxy.GetHandle();

  // receive response from server
  if (myhandle->RecvFromServer(new_response, recv_msg, info)== -1){
    // send 502 to client
    string msg = "HTTP/1.1 502 Bad Gateway";
    if (send(info.m_client_socket, msg.c_str(), msg.length(), MSG_NOSIGNAL) == -1){
      proxy.m_log.PrintError("Fail to inform client having problem receiving from server!", info.m_uid);
      return -1;
    }
    proxy.m_log.PrintError("Fail to receive response from server", info.m_uid);
    return -1;
  }
  // parse the response, and assign the new_response
  if(new_response.parseResponse(recv_msg, new_response, info) == -1){
    proxy.m_log.PrintError("Ill-formatted response from server ", info.m_uid);
    // send 502 to client
    string msg = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
    if (send(info.m_client_socket, msg.c_str(), msg.length(), MSG_NOSIGNAL) == -1){
      proxy.m_log.PrintError("Fail to inform client having problem parsing response from server!", info.m_uid);
      return -1;
    }
    return -1;
  }

  //ID: Received "RESPONSE" from SERVER
  proxy.m_log.Print("Received " + new_response.getStatusLine() + " from " + info.m_host, info.m_uid);
  // check if it needs to be revalidated, and print the log
  if (new_response.getStatus() == "200") {
    //Check if cacheable
    if (new_response.getIsCacheable()) {
      string currTime = proxy.GetHandle()->GetCurTime();
      cache.setResponse(info.m_req_line, new_response, currTime);
      // check if it needs to be revalidated, and print the log
if (proxy.GetHandle()->needRevalidation(old_response, new_response)){
  proxy.m_log.Print("cached, but requires re-validation", info.m_uid);
  // send new response to client
  if (send(info.m_client_socket, new_response.recv_msg.data(), new_response.recv_msg.size(), MSG_NOSIGNAL) <= 0){
    proxy.m_log.PrintError("Fail to send response to client", info.m_uid);
    return -1;
  }
  //ID: Responding "RESPONSE"
  proxy.m_log.Print("Responding " + new_response.getStatusLine(), info.m_uid);
  return 0;
}
else {
/////////// Cacheable but not sure if it needs to be revalidated///////
      if (new_response.cache_control.find("max-age") != string::npos) {
        proxy.m_log.Print("found max-age", info.m_uid);
        // get max-age
        string max_age;
        size_t pos = new_response.cache_control.find("max-age");
        if (pos != std::string::npos) {
            // if max-age is the last field
            if (new_response.cache_control.find(",") == string::npos) {
                max_age = new_response.cache_control.substr(pos + 7);
            }
            else {
                max_age = new_response.cache_control.substr(pos + 7, new_response.cache_control.find(",") - pos - 7);
            }
        }
        // there is no last_modified field, but max-age is 0
        if (max_age == "0") {
          proxy.m_log.Print("max-age = 0", info.m_uid);
            proxy.m_log.Print("cached, but requires re-validation", info.m_uid);
        }
        
        // if last_modified is not empty
        // If now - last_modified > max_age, need revalidate
        if (!new_response.last_modified.empty()) {
          proxy.m_log.Print("last_modified is not empty", info.m_uid);
            time_t now = time(0);
            struct tm tm;
            strptime(new_response.last_modified.c_str(), "%a, %d %b %Y %H:%M:%S %Z", &tm);
            time_t last_modified_time = mktime(&tm);
            //calculate expire_time
            time_t expires_time = last_modified_time + stoi(max_age);
            struct tm *expires_tm = gmtime(&expires_time);
            char expire_time[100];
            strftime(expire_time, 100, "%a, %d %b %Y %H:%M:%S %Z", expires_tm);

            if (difftime(now, last_modified_time) > stoi(max_age)) {
              //check if max-stale is in new_response.cache_control
              if (new_response.cache_control.find("max-stale") != string::npos) {
                //get max-stale
                string max_stale;
                size_t pos = new_response.cache_control.find("max-stale");
                if (pos != std::string::npos) {
                  // if max-stale is the last field
                  if (new_response.cache_control.find(",") == string::npos) {
                    max_stale = new_response.cache_control.substr(pos + 9);
                  }
                  else {
                    max_stale = new_response.cache_control.substr(pos + 9, new_response.cache_control.find(",") - pos - 9);
                  }
                }
                //if max-stale is empty, then use the default value
                if (max_stale.empty()) {
                  proxy.m_log.Print("cached but requires revalidation ", info.m_uid);

                }
                //if max-stale is not empty, then check if now - last_modified > max_age + max_stale
                else {
                  if (difftime(now, last_modified_time) > stoi(max_age) + stoi(max_stale)) {
                    //update expire_time
                    expires_time = last_modified_time + stoi(max_age) + stoi(max_stale);
                    proxy.m_log.Print("cached but requires revalidation ", info.m_uid);
                  }
                  else {
                    proxy.m_log.Print("cached, expires at Expires: " + string(expire_time), info.m_uid);
                  }
                }
            }
            //ID: cached, expired at$EXPIRES$$
            else {
                proxy.m_log.Print("cached but requires revalidation " , info.m_uid);

            } 
            
        }
        else{
          proxy.m_log.Print("cached, expires at" + string(expire_time), info.m_uid);
        }

    }
  }
      // if expires is not empty
    // if now > expires, need revalidate
    else if (!new_response.expires.empty()) {
        time_t now = time(0);
        struct tm tm;
        strptime(new_response.expires.c_str(), "%a, %d %b %Y %H:%M:%S %Z", &tm);
        time_t expires_time = mktime(&tm);
        if (difftime(now, expires_time) >= 0) {
            proxy.m_log.Print("cached but requires revalidation ", info.m_uid);
        }
        //ID: cached, expires at$EXPIRES$$
        else {
            proxy.m_log.Print("cached, expires at" + new_response.expires, info.m_uid);
        }
    }
          // if there is no field, need revalidate
    else {
        proxy.m_log.Print("cached but requires revalidation", info.m_uid);
        }

/////////// Cacheable but not sure if it needs to be revalidated///////



    // send response to client
    if (send(info.m_client_socket, new_response.recv_msg.data(), new_response.recv_msg.size(), MSG_NOSIGNAL) <= 0){
      proxy.m_log.PrintError("Fail to send response to client", info.m_uid);
      return -1;
    }
    //ID: Responding "RESPONSE"
    proxy.m_log.Print("Responding " + new_response.getStatusLine(), info.m_uid);
    return 0;
  }
}
    // not cacheable
    else {
      // not cacheable because of no-store    
    if (new_response.cache_control.find("no-store") != string::npos) {
        proxy.m_log.Print("not cacheable because no-store", info.m_uid);
    }
    else if (new_response.cache_control.find("private") != string::npos) {
        proxy.m_log.Print("not cacheable because private", info.m_uid);
    }
    // send response to client
    if (send(info.m_client_socket, new_response.recv_msg.data(), new_response.recv_msg.size(), MSG_NOSIGNAL) <= 0){
      proxy.m_log.PrintError("Fail to send response to client", info.m_uid);
      return -1;
    }
    //ID: Responding "RESPONSE"
    proxy.m_log.Print("Responding " + new_response.getStatusLine(), info.m_uid);
    return 0;
    }
  }

// status is "304"
else if (new_response.getStatus() == "304") {
  // send old response to client
  if (send(info.m_client_socket, old_response.recv_msg.data(), old_response.recv_msg.size(), MSG_NOSIGNAL) <= 0){
    proxy.m_log.PrintError("Fail to send response to client", info.m_uid);
    return -1;
  }
  //ID: Responding "RESPONSE"
  proxy.m_log.Print("Responding " + old_response.getStatusLine(), info.m_uid);
  return 0;
}   
return 0;

}


bool MyHandle::needRevalidation(Response &old_response, Response &new_response) {
  // Check if any of the fields exist in both old and new response
  bool has_if_modified_since = !old_response.last_modified.empty() && !new_response.last_modified.empty();
  bool has_if_none_match = !old_response.etag.empty() && !new_response.etag.empty();
  bool has_etag = !old_response.etag.empty() && !new_response.etag.empty();
  bool has_last_modified = !old_response.last_modified.empty() && !new_response.last_modified.empty();

  // If all fields are empty, we cannot revalidate
  if (!has_if_modified_since && !has_if_none_match && !has_etag && !has_last_modified) {
    return false;
  }
  // Check if the fields match between old and new response
  if (has_if_modified_since) {
    if (old_response.last_modified != new_response.last_modified) {
      return true;
    }
  }
  if (has_if_none_match) {
    if (old_response.etag != new_response.etag) {
      return true;
    }
  }
  if (has_etag) {
    if (old_response.etag != new_response.etag) {
      return true;
    }
  }
  if (has_last_modified) {
    if (old_response.last_modified != new_response.last_modified) {
      return true;
    }
  }
  return false;
}



#include "MyProxy.h"


using namespace std;

// Example HTTP server response:
//
// HTTP/1.1 200 OK
// Date: Sat, 27 Feb 2023 12:34:56 GMT
// Server: Apache/2.4.6 (CentOS) OpenSSL/1.0.2k-fips PHP/7.2.23
// Cache-Control: max-age=3600
// Expires: Sat, 27 Feb 2023 13:34:56 GMT
// Last-Modified: Wed, 22 Feb 2023 10:00:00 GMT
// ETag: "1234567890abcdef"
// Content-Length: 1234
// Content-Type: text/html; charset=UTF-8
// 
// <!DOCTYPE html>
// <html>
//   <head>
//     <title>Example Page</title>
//   </head>
//   <body>
//     <h1>Hello, World!</h1>
//     <p>This is an example page.</p>
//   </body>
// </html>

int Response::parseResponse(vector<char> &response_msg, Response &response, ConnectInfo &info) {
    // get head
    // Response::GetHttpHead(response_msg, response.head);
    if (Response::GetHttpHead(response_msg, response.head) == -1) {
        proxy.m_log.PrintError("parse response head error");
        return -1;
    }

    /* Must be present fields*/
    // get status
    if (Response::GetRequestLine(response.head, response.statusLine) == -1) {
        proxy.m_log.PrintError("parse response status line error");
        return -1;
    }
    else {

        // extract status code
        string status_code;
        size_t pos = statusLine.find(" ");
        if (pos != std::string::npos) {
            status = statusLine.substr(pos + 1, 3);
            response.status = status;
  
        }
    }
    // get date
    if (GetHttpHeadParam(response.head, "Date", response.date) == -1) {
        proxy.m_log.PrintError("parse response date error");
        return -1;
    }

    // get server
    if (GetHttpHeadParam(response.head, "Server", response.server) == -1) {
        proxy.m_log.PrintError("parse response server error");
        return -1;
    }

    string response_msg_str(response_msg.begin(), response_msg.end());

    if (response_msg_str.find("Transfer-Encoding: chunked") != string::npos) {
        isChunked = true;
    }
    else {

        // get content_length
        if (GetHttpHeadParam(response.head, "Content-Length", response.content_length) == -1) {
            proxy.m_log.PrintError("parse response content length error");
            return -1;
        }
 
    }

    

    // get content_type
    if (GetHttpHeadParam(response.head, "Content-Type", response.content_type) == -1) {
        proxy.m_log.PrintError("parse response content type error");
        return -1;
    }


    // get cache_control
    if (GetHttpHeadParam(response.head, "Cache-Control", response.cache_control) == 0) {
        // check if the response is cacheable for proxy
        setCacheable(response.isCacheable);
    }


    // get expires, which is optional
    GetHttpHeadParam(response.head, "Expires", response.expires);


    // get last_modified
    GetHttpHeadParam(response.head, "Last-Modified", response.last_modified);

    // get etag
    GetHttpHeadParam(response.head, "ETag", response.etag);

    //If-None-Match
    GetHttpHeadParam(response.head, "If-None-Match", response.if_none_match);

    //If-Modified-Since
    GetHttpHeadParam(response.head, "If-Modified-Since", response.if_modified_since);



    //copy vector<char> recv_msg
    response.recv_msg.insert(response.recv_msg.end(), response_msg.begin(), response_msg.end());
    
    if (Response::GetHttpBody(response_msg, response, info) == -1) {
    return -1;
    }
    
    return 0;
}

// Check if the response is cacheable for proxy, and set the corresponding field
void Response::setCacheable(bool &isCacheable) {
    if (cache_control.find("no-store") != string::npos || cache_control.find("private") != string::npos) {
        isCacheable = false;
        
    } 
   isCacheable = true;
}

// Check if need to revalidate the response
bool Response::needRevalidate(ConnectInfo &info){
    if (!isCacheable) {
        // ID: not cacheable because$REASON$
        if (cache_control.find("no-cache") != string::npos) {
            proxy.m_log.Print("not cacheable because no-cache", info.m_uid);
        }
        else if (cache_control.find("no-store") != string::npos) {
            proxy.m_log.Print("not cacheable because no-store", info.m_uid);
        }
        else if (cache_control.find("private") != string::npos) {
            proxy.m_log.Print("not cacheable because private", info.m_uid);
        }
        else if (head.find("Set-Cookie") != string::npos) {
            proxy.m_log.Print("not cacheable because Set-Cookie", info.m_uid);
        }
        return true;
    }
    else if (cache_control.find("must-revalidate") != string::npos) {
        proxy.m_log.Print("cached, but requires re-validation", info.m_uid);
        return true;
    }
    else if (cache_control.find("proxy-revalidate") != string::npos) {
        proxy.m_log.Print("cached, but requires re-validation", info.m_uid);
        return true;
    }
    else if (cache_control.find("max-age") != string::npos) {
        // get max-age
        string max_age;
        size_t pos = cache_control.find("max-age");
        if (pos != std::string::npos) {
            // if max-age is the last field
            if (cache_control.find(",") == string::npos) {
                max_age = cache_control.substr(pos + 7);
            }
            else {
                max_age = cache_control.substr(pos + 7, cache_control.find(",") - pos - 7);
            }
        }
        
        // if last_modified is not empty
        // If now - last_modified > max_age, need revalidate
        if (!last_modified.empty()) {
            time_t now = time(0);
            struct tm tm;
            strptime(last_modified.c_str(), "%a, %d %b %Y %H:%M:%S %Z", &tm);
            time_t last_modified_time = mktime(&tm);
            if (difftime(now, last_modified_time) > stoi(max_age)) {
                proxy.m_log.Print("cached, but requires re-validation", info.m_uid);
                return true;
            }
            //ID: cached, expires at$EXPIRES$$
            else {
                //calculate expire_time
                time_t expires_time = last_modified_time + stoi(max_age);
                struct tm *expires_tm = gmtime(&expires_time);
                char expire_time[100];
                strftime(expire_time, 100, "%a, %d %b %Y %H:%M:%S %Z", expires_tm);
                proxy.m_log.Print("cached, expires at " + string(expire_time), info.m_uid);
                return false;

            } 
        }

        // there is no last_modified field, but max-age is 0
        if (max_age == "0") {
            proxy.m_log.Print("cached, but requires re-validation", info.m_uid);
            return true;
        }

    }

    // if expires is not empty
    // if now > expires, need revalidate
    else if (!expires.empty()) {
        time_t now = time(0);
        struct tm tm;
        strptime(expires.c_str(), "%a, %d %b %Y %H:%M:%S %Z", &tm);
        time_t expires_time = mktime(&tm);
        if (difftime(now, expires_time) >= 0) {
            proxy.m_log.Print("cached, but requires re-validation", info.m_uid);
            return true;
        }
        //ID: cached, expires at$EXPIRES$$
        else {
            proxy.m_log.Print("cached, expires at " + expires, info.m_uid);
            return false;
        }
    }
    // if there is no expires field, and no

    return false;
}

// Check if GET request is cached
bool Response::reqCached(ConnectInfo &info){
    // ID: cached, but requires re-validation$$
    if (cache_control.find("must-revalidate") != string::npos) {
        proxy.m_log.Print("cached, but requires re-validation", info.m_uid);
        return true;
    }
    else if (cache_control.find("proxy-revalidate") != string::npos) {
        proxy.m_log.Print("cached, but requires re-validation", info.m_uid);
        return true;
    }
    else if (cache_control.find("max-age") != string::npos) {
        // get max-age
        string max_age;
        size_t pos = cache_control.find("max-age");
        if (pos != std::string::npos) {
            // if max-age is the last field
            if (cache_control.find(",") == string::npos) {
                max_age = cache_control.substr(pos + 7);
            }
            else {
                max_age = cache_control.substr(pos + 7, cache_control.find(",") - pos - 7);
            }
        }
        
        // if last_modified is not empty
        // If now - last_modified > max_age, need revalidate
        if (!last_modified.empty()) {
            time_t now = time(0);
            struct tm tm;
            strptime(last_modified.c_str(), "%a, %d %b %Y %H:%M:%S %Z", &tm);
            time_t last_modified_time = mktime(&tm);
            if (difftime(now, last_modified_time) > stoi(max_age)) {
                proxy.m_log.Print("cached, but requires re-validation", info.m_uid);
                return true;
            }
            //ID: cached, expires at$EXPIRES$$
            else {
                //calculate expire_time
                time_t expires_time = last_modified_time + stoi(max_age);
                struct tm *expires_tm = gmtime(&expires_time);
                char expire_time[100];
                strftime(expire_time, 100, "%a, %d %b %Y %H:%M:%S %Z", expires_tm);
                proxy.m_log.Print("cached, expires at " + string(expire_time), info.m_uid);
                return false;

            } 
        }

        // there is no last_modified field, but max-age is 0
        if (max_age == "0") {
            proxy.m_log.Print("cached, but requires re-validation", info.m_uid);
            return true;
        }

    }

    // if expires is not empty
    // if now > expires, need revalidate
    else if (!expires.empty()) {
        time_t now = time(0);
        struct tm tm;
        strptime(expires.c_str(), "%a, %d %b %Y %H:%M:%S %Z", &tm);
        time_t expires_time = mktime(&tm);
        if (difftime(now, expires_time) >= 0) {
            proxy.m_log.Print("cached, but requires re-validation", info.m_uid);
            return true;
        }
        //ID: cached, expires at$EXPIRES$$
        else {
            proxy.m_log.Print("cached, expires at " + expires, info.m_uid);
            return false;
        }
    }
    // if there is no expires field, and no

    // logFile << info.m_uid <<": in cache, valid" << endl;
    proxy.m_log.Print("in cache, valid", info.m_uid);
    return false;

}



int Response::GetHttpHead(vector<char> & req_msg, string & http_head) {
  size_t head_size;

  head_size = search(req_msg.begin(), req_msg.end(), std::vector<char>{'\r', '\n', '\r', '\n'}.begin(), std::vector<char>{'\r', '\n', '\r', '\n'}.end()) - req_msg.begin();
  if (head_size == string::npos)
    return -1;
  http_head = string(req_msg.begin(), req_msg.begin() + head_size);
  return 0;
}

int Response::GetHttpBody(vector<char>& req_msg, Response &response, ConnectInfo &info) {
  // Get content length from HTTP header
    //change req_msg type to vector<char>
  string len;
  if (response.GetHttpHeadParam(response.head, "Content-Length", len) == -1) {
    proxy.m_log.PrintError("GET: fail to get http body");
    return -1;
  }
  response.body = string(req_msg.begin() + info.m_http_head.length() + 4, req_msg.begin() + info.m_http_head.length() + 4 + atoi(len.c_str()));
  return 0;
}

// int Response::GetHttpBody(vector<char>& req_msg, Response &response, ConnectInfo &info) {
//   // Get content length from HTTP header
//   string len;
//   if (response.GetHttpHeadParam(response.head, "Content-Length", len) == -1) {
//     proxy.m_log.PrintError("GET reponse: fail to get http body");
//     return -1;
//   }
//   int content_length = atoi(len.c_str());
//   proxy.m_log.Print("GetHttpBody: content_length = " + to_string(content_length), info.m_uid);
  
//   // Determine how much data has already been received
//   int bytes_received = req_msg.size() - response.head.length() - 4;
  
//   // Keep receiving data until full HTTP message is received
//   while (bytes_received < content_length) {
//     char buf[65535];
//     int bytes_read = recv(info.m_client_socket, buf, sizeof(buf), 0);
//     if (bytes_read == -1) {
//       proxy.m_log.PrintError("GET response: Error receiving body from client");
//       return -1;
//     }
//     bytes_received += bytes_read;
//     req_msg.insert(req_msg.end(), buf, buf + bytes_read);
//   }
//   proxy.m_log.Print("GetHttpBody: out of while loop", info.m_uid);
  
//   // Extract HTTP body from full message
//   response.body = string(req_msg.begin() + response.head.length() + 4, req_msg.end());
//   return 0;
// }

// int Response::GetHttpBody(vector<char>& req_msg, Response &response, ConnectInfo &info) {
//   // If response is chunked, receive chunks until end of message
//   if (response.isChunked) {
//     string chunk_length_str;
//     int chunk_length = 0;

//     // Keep reading chunks until 0-length chunk is received
//     while (true) {
//       // Read chunk length
//       int pos = req_msg.size();
//       while (pos < (int)req_msg.size() + 64) {
//         char c;
//         int bytes_read = recv(info.m_server_socket, &c, 1, 0);
//         if (bytes_read != 1) {
//           proxy.m_log.PrintError("GET response: Error receiving chunk length from server");
//           return -1;
//         }
//         if (c == '\r') {
//           break;
//         }
//         chunk_length_str += c;
//         pos++;
//       }
//       if (chunk_length_str.empty()) {
//         break;
//       }
//       chunk_length = stoi(chunk_length_str, 0, 16);

//       // If end of message is reached, exit loop
//       if (chunk_length == 0) {
//         break;
//       }

//       // Receive chunk and append to HTTP body
//       while (req_msg.size() < pos + chunk_length + 2) {
//         char buf[65535];
//         int bytes_read = recv(info.m_server_socket, buf, sizeof(buf), 0);
//         if (bytes_read == -1) {
//           proxy.m_log.PrintError("GET response: Error receiving chunk from server");
//           return -1;
//         }
//         req_msg.insert(req_msg.end(), buf, buf + bytes_read);
//       }
//       response.body += string(req_msg.begin() + pos, req_msg.begin() + pos + chunk_length);
//       req_msg.erase(req_msg.begin() + pos, req_msg.begin() + pos + chunk_length + 2);

//       // Reset chunk length for next chunk
//       chunk_length_str.clear();
//       chunk_length = 0;
//     }
//   } else {
//     // If response is not chunked, get content length from HTTP header
//     string len;
//     if (response.GetHttpHeadParam(response.head, "Content-Length", len) == -1) {
//       proxy.m_log.PrintError("GET response: fail to get http body");
//       return -1;
//     }
//     int content_length = atoi(len.c_str());

//     // Determine how much data has already been received
//     int bytes_received = req_msg.size() - response.head.length() - 4;

//     // Keep receiving data until full HTTP message is received
//     while (bytes_received < content_length) {
//       char buf[65535];
//       int bytes_read = recv(info.m_server_socket, buf, sizeof(buf), 0);
//       if (bytes_read == -1) {
//         proxy.m_log.PrintError("GET response: Error receiving body from client");
//         return -1;
//       }
//       bytes_received += bytes_read;
//       req_msg.insert(req_msg.end(), buf, buf + bytes_read);
//     }

//     // Extract HTTP body from full message
//     response.body = string(req_msg.begin() + response.head.length() + 4, req_msg.end());
//   }

//   return 0;
// }


// req_msg type changed from string to vector<char>
int Response::GetRequestLine(string & http_head, string & req_line) {
  size_t end_pos;

  end_pos = http_head.find("\r\n");
  if (end_pos == string::npos)
    return -1;
  req_line = http_head.substr(0, end_pos);
  return 0;
}










int Response::GetHttpHeadParam(string & http_head, string name, string & value) {
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

bool Response::paraExist(string& head, string key, string value = "") {
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



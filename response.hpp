#ifndef RESPONSE_H
#define RESPONSE_H

#include <string>
#include <iostream>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <algorithm>

#include "connectInfo.hpp"





/* Create HTTP Server Response class and parse it*/
/* Example: HTTP/1.1 200 OK
Date: Sat, 27 Feb 2023 12:34:56 GMT
Server: Apache/2.4.6 (CentOS) OpenSSL/1.0.2k-fips PHP/7.2.23
Cache-Control: max-age=3600
Expires: Sat, 27 Feb 2023 13:34:56 GMT
Last-Modified: Wed, 22 Feb 2023 10:00:00 GMT
ETag: "1234567890abcdef"
Content-Length: 1234
Content-Type: text/html; charset=UTF-8
*/

class Response {
    public: 
        std::vector<char> recv_msg;
        std::string statusLine;
        std::string status;
        std::string date;
        std::string server;
        std::string cache_control;
        std::string expires;
        std::string last_modified;
        std::string etag;
        std::string content_length;
        std::string content_type;
        std::string transfer_encoding;
        std::string head;
        std::string body;
        std::string if_none_match;
        std::string if_modified_since;
        bool isChunked;
        bool isCacheable;

    public:
        Response(): isChunked(false), isCacheable(true) {}
        // copy constructor
        Response(const Response &response){
            statusLine = response.statusLine;
            status = response.status;
            date = response.date;
            server = response.server;
            cache_control = response.cache_control;
            expires = response.expires;
            last_modified = response.last_modified;
            etag = response.etag;
            content_length = response.content_length;
            content_type = response.content_type;
            transfer_encoding = response.transfer_encoding;
            head = response.head;
            body = response.body;
            isChunked = response.isChunked;
            isCacheable = response.isCacheable;
            //copy vector<char> recv_msg
            recv_msg.insert(recv_msg.end(), response.recv_msg.begin(), response.recv_msg.end());
        }
        // assignment operator
        Response& operator=(const Response &response) {
            if (this != &response) {
                statusLine = response.statusLine;
                status = response.status;
                date = response.date;
                server = response.server;
                cache_control = response.cache_control;
                expires = response.expires;
                last_modified = response.last_modified;
                etag = response.etag;
                content_length = response.content_length;
                content_type = response.content_type;
                transfer_encoding = response.transfer_encoding;
                head = response.head;
                body = response.body;
                isChunked = response.isChunked;
                isCacheable = response.isCacheable;
                //assign vector<char> recv_msg 
                recv_msg.insert(recv_msg.end(), response.recv_msg.begin(), response.recv_msg.end());
            }
            return *this;
        }


        std::vector<char> getRecvMsg() { return recv_msg; }
        std::string getStatusLine() { return statusLine; }
        std::string getStatus() { return status; }
        std::string getDate() { return date; }
        std::string getServer() { return server; }
        std::string getCacheControl() { return cache_control; }
        std::string getExpires() { return expires; }
        std::string getLastModified() { return last_modified; }
        std::string getEtag() { return etag; }
        std::string getContentLength() { return content_length; }
        std::string getContentType() { return content_type; }
        std::string getTransferEncoding() { return transfer_encoding; }
        std::string getHead() { return head; }
        std::string getBody() { return body; }
        bool getIsChunked() { return isChunked; }
        bool getIsCacheable() { return isCacheable; }

        // parse response and set the corresponding fields
        int parseResponse(std::vector<char> & req_msg, Response &response, ConnectInfo &info);

        // set the isCacheable field
        void setCacheable(bool &isCacheable);

        // check if need revalidation
        bool needRevalidate(ConnectInfo &info);

        //check if GET request is cached
        bool reqCached(ConnectInfo &info);


        static int GetHttpHead(std::vector<char> & req_msg, std::string & http_head);  // get http head
        static int GetHttpBody(vector<char>& req_msg, Response &response, ConnectInfo &info); // get http body
        static int GetRequestLine(std::string & http_head, std::string & req_line);  // get request line
        int GetHttpHeadParam(std::string & http_head, std::string name, std::string & value);
        bool paraExist(string& head, string key, string value);

        

};

#endif

#ifndef CACHE_HPP
#define CACHE_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <list>
#include <fstream>
#include <iterator>
#include <mutex>

#include "response.hpp"


/* LRU cache, implemented hash table. */
/* proxy MUST cache responses (when they are 200-OK) to GET requests. You should follow the rules of expiration time and/or re-validation in determining if your proxy can serve a request from its local cache (versus re-fetching from the origin server).
*/

class Cache {
    private:
        class NodeCache {
            public:
                std::string req_line;
                Response response;

                NodeCache() {}
                NodeCache(std::string &req_line, Response &response): req_line(req_line), response(response) {}
                // copy constructor
                NodeCache(const NodeCache &node): req_line(node.req_line), response(node.response) {}
                // assignment operator
                NodeCache& operator=(const NodeCache &node) {
                    if(this != &node) {
                        req_line = node.req_line;
                        response = node.response;
                    }
                    return *this;
                }
                // destructor
                ~NodeCache() {}

                //get the request line
                std::string getReqLine() {
                    return req_line;
                }
                // get the response
                Response getResponse() {
                    return response;
                }

        };
        std::unordered_map<std::string, std::list<NodeCache>::iterator> cacheMap;
        std::list<NodeCache> cacheList;
        int cacheSize;
        int cacheCapacity;
        int cacheHit;
        int cacheMiss;
        int cacheEvict;
        int cacheUpdate;
        std::mutex cacheMutex;

    public:
        Cache(int cacheSize): cacheSize(cacheSize), cacheCapacity(0), cacheHit(0), cacheMiss(0), cacheEvict(0), cacheUpdate(0) {}

        // get the response from cache
        Response getResponse(std::string &req_line);

        // set the response to cache
        void setResponse(std::string &req_line, Response &response, std::string currTime);

        // update the response in cache
        void updateResponse(std::string &req_line, Response &response, std::string currTime);

        // evict the response from cache
        void evictResponse(std::string &req_line);

        // print the cache
        void printCache();

        // print the cache statistics
        void printCacheStatistics();


};

#endif
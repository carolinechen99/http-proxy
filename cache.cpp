# include "MyProxy.h"
using namespace std;

// get the response from cache
Response Cache::getResponse(std::string &req_line){
    std::lock_guard<std::mutex> lock(cacheMutex);
    if (cacheMap.find(req_line) != cacheMap.end()) {
        // initialize the result
        Response result;
        cacheHit++;
        // result is the value of the key req_line
        result = cacheMap[req_line]->response;
        // move the node to the front of the list
        cacheList.splice(cacheList.begin(), cacheList, cacheMap[req_line]);
        // update the iterator in the map
        cacheMap[req_line] = cacheList.begin();
        return result;
    } else {
        cacheMiss++;
        return Response();
    }
}

// set the response to cache
void Cache::setResponse(std::string &req_line, Response &response, std::string currTime){
    // std::lock_guard<std::mutex> lock(cacheMutex);
    if (cacheMap.find(req_line) != cacheMap.end()) {
        // update the response in cache
        updateResponse(req_line, response, currTime);
    } else {
        // evict the response from cache
        if (cacheCapacity == cacheSize) {
            evictResponse(cacheList.back().req_line);
        }
        // insert the response to cache
        cacheList.push_front(NodeCache(req_line, response));
        cacheMap[req_line] = cacheList.begin();
        cacheCapacity++;
        cacheUpdate++;
    }

}

// update the response in cache
void Cache::updateResponse(std::string &req_line, Response &response, std::string currTime){
    // std::lock_guard<std::mutex> lock(cacheMutex);
    cacheMap[req_line]->response = response;
    // update the time
    cacheMap[req_line]->response.last_modified = currTime;
    // move the node to the front of the list
    cacheList.splice(cacheList.begin(), cacheList, cacheMap[req_line]);
    // update the iterator in the map
    cacheMap[req_line] = cacheList.begin();
    cacheUpdate++;
}

// evict the response from cache
void Cache::evictResponse(std::string &req_line){
    // std::lock_guard<std::mutex> lock(cacheMutex);
    cacheMap.erase(req_line);
    cacheList.pop_back();
    cacheCapacity--;
    cacheEvict++;
}

// print the cache for DEBUG
void Cache::printCache(){
    std::cout << "Cache:" << std::endl;
    for (list<NodeCache>::iterator it = cacheList.begin(); it != cacheList.end(); it++) {
        std::cout << it->req_line << std::endl;
    }
}

// print the cache statistics for DEBUG
void Cache::printCacheStatistics(){
    std::cout << "Cache Statistics:" << std::endl;
    std::cout << "Cache Hit: " << cacheHit << std::endl;
    std::cout << "Cache Miss: " << cacheMiss << std::endl;
    std::cout << "Cache Evict: " << cacheEvict << std::endl;
    std::cout << "Cache Update: " << cacheUpdate << std::endl;
}


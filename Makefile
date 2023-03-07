all: MyProxy
MyProxy: MyProxy.cpp MyProxy.h MyLog.cpp MyLog.h MyHandle.cpp MyHandle.h cache.cpp cache.hpp response.cpp response.hpp connectInfo.hpp
		g++ -g -o MyProxy MyProxy.cpp MyProxy.h MyLog.cpp MyLog.h MyHandle.cpp MyHandle.h cache.cpp cache.hpp response.cpp response.hpp connectInfo.hpp  -lpthread
.PHONY:
		clean
clean:
		rm -rf *.o MyProxy

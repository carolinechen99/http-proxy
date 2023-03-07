#include "MyProxy.h"

MyProxy proxy;
Cache cache(10000000);

int main() {
  if (!proxy.Init()) {
    cout << "MyProxy init failed!" << endl;
    return -1;
  }
  return proxy.Run("12345");
}

MyProxy::MyProxy(): m_cache(100) {
}

MyProxy::~MyProxy() {
}


bool MyProxy::Init() {
  if (!m_log.Init("proxy.log"))
    return false;
  return true;
}

int MyProxy::Run(const string & port) {
  struct addrinfo * res;
  int hSocket;
  int unq_id = 1;
  string ip;
  ConnectInfo * newInfo;
  pthread_t hThread;

  // create a socket, bind the port and start listening
  if (CreateSocket(NULL, port, &res, &hSocket) || SetBindSocket(res, &hSocket) ||
      StartListen(hSocket))
    return -1;
  m_log.PrintNote("MyProxy is listening... port: " + port);

  // loop to establish connection
  while (unq_id) {
    newInfo = AcceptSocket(hSocket, unq_id++);
    if (pthread_create(&hThread, NULL, (pThreadFunc)m_handle.HandleFunc, newInfo))
      delete newInfo;
  }

  // shut down socket and free
  shutdown(hSocket, SHUT_RDWR);
  freeaddrinfo(res);
  m_log.PrintNote("MyProxy Stop!");
  return 0;
}

int MyProxy::CreateSocket(const char * host,
                          const string & port,
                          struct addrinfo ** res_ptr,
                          int * hSocket) {
  struct addrinfo hints = {0};
  struct addrinfo * res;

  hints.ai_family = PF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_IP;
  hints.ai_flags = AI_PASSIVE;

  if (getaddrinfo(host, port.data(), &hints, res_ptr)) {
    m_log.PrintError("Fail to get addr info!");
    return -1;
  }

  res = *res_ptr;
  *hSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (*hSocket == -1) {
    m_log.PrintError("Fail to create socket!");
    return -1;
  }
  return 0;
}

int MyProxy::SetBindSocket(addrinfo * res, int * hSocket) {
  int one = 1;

  if (setsockopt(*hSocket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int))) {
    m_log.PrintError("Fail to set socket fd's option!");
    return -1;
  }
  if (bind(*hSocket, res->ai_addr, res->ai_addrlen)) {
    m_log.PrintError("Fail to bind socket!");
    return -1;
  }
  return 0;
}

int MyProxy::StartListen(int hSocket) {
  if (listen(hSocket, SOMAXCONN)) {
    m_log.PrintError("Fail to start listening!");
    return -1;
  }
  return 0;
}

ConnectInfo * MyProxy::AcceptSocket(int hSocket, int unq_id) {
  int newSocket;
  socklen_t addrLen = sizeof(sockaddr_storage);
  struct sockaddr_storage addr;
  ConnectInfo * newInfo;

  newSocket = accept(hSocket, (struct sockaddr *)&addr, &addrLen);
  if (newSocket == -1) {
    m_log.PrintError("Fail to accept!");
    return NULL;
  }

  // create new connection information to process sub-thread
  newInfo = new ConnectInfo(
      unq_id, newSocket, inet_ntoa(((struct sockaddr_in *)(&addr))->sin_addr));

  // multi-thread

  return newInfo;
}




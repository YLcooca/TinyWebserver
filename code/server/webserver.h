#pragma once

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <unordered_map>

#include "../http/connection.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.hpp"
#include "../utility/timer.h"
#include "epoller.h"

class WebServer {
 public:
  WebServer(int port, int trig_mode, int timeout, bool opt_linger, int sql_port,
            const char *sql_user, const char *sql_pwd, const char *dbname,
            int connpool_num, int thread_num, bool openlog, int log_level,
            int log_queue_size);
  ~WebServer();
  void start();

 private:
  bool initSocket();
  void initEventMode(int trig_mode);
  void addClient(int fd, sockaddr_in addr);

  void dealListen();
  void dealWrtie(HttpConn *client);
  void dealRead(HttpConn *client);

  void sendError(int fd, const char *info);
  void extentTime(HttpConn *client);
  void closeConn(HttpConn *client);

  void onRead(HttpConn *client);
  void onWrite(HttpConn *client);
  void onProcess(HttpConn *client);

  static const int MAX_FD = 65535;
  static int setFdNonblock(int fd);

  int port_;
  bool open_linger_;
  int timeout_ms_;
  bool is_close_;
  int listen_fd_;
  char *src_dir_;

  uint32_t listen_event_;
  uint32_t conn_event_;

  std::unique_ptr<HeapTimer> timer_;
  std::unique_ptr<Epoller> epoller_;
  std::unique_ptr<ThreadPool> threadpool_;
  std::unordered_map<int, HttpConn> users_;
};
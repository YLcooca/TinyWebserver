#pragma once

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <cerrno>
#include <cstdlib>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnRAII.h"
#include "request.h"
#include "response.h"

class HttpConn {
 public:
  HttpConn() = default;
  ~HttpConn();

  void init(int sockfd, const sockaddr_in &addr);
  ssize_t read(int *save_errno);
  ssize_t write(int *save_errno);

  void closeConn();
  int getFd() const;
  int getPort() const;
  const char *getIP() const;
  sockaddr_in getAddr() const;

  bool process();
  int toWriteBytes() { return iov_[0].iov_len + iov_[1].iov_len; }
  bool isKeepalive() const { return request_.isKeepalive(); }

  static bool is_et_;
  static const char *src_dir_;
  static std::atomic<int> user_count_;

 private:
  int fd_{-1};
  sockaddr_in addr_{0};
  bool is_close_{true};
  int iov_cnt_;
  iovec iov_[2];

  Buffer read_buffer_;
  Buffer write_buffer_;

  HttpRequest request_;
  HttpResponse response_;
};
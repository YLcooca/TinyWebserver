#include "connection.h"

const char *HttpConn::src_dir_{""};
std::atomic<int> HttpConn ::user_count_{0};
bool HttpConn::is_et_{false};

HttpConn::~HttpConn() { closeConn(); }

void HttpConn::init(int fd, const sockaddr_in &addr) {
  assert(fd > 0);
  user_count_++;
  addr_ = addr;
  write_buffer_.RetrieveAll();
  read_buffer_.RetrieveAll();
  is_close_ = false;
  LOG_INFO("client[%d](%s:%d) in", fd_, getIP(), getPort());
}

void HttpConn::closeConn() {
  response_.UnmapFile();
  if (is_close_ == false) {
    is_close_ = true;
    user_count_--;
    close(fd_);
    LOG_INFO("client[%d](%s:%d) quit", fd_, getIP(), getPort());
  }
}

int HttpConn::getFd() const { return fd_; }

sockaddr_in HttpConn::getAddr() const { return addr_; }

const char *HttpConn::getIP() const { return inet_ntoa(addr_.sin_addr); }

int HttpConn::getPort() const { return addr_.sin_port; }

ssize_t HttpConn::read(int *save_errno) {
  ssize_t len = -1;
  do {
    len = read_buffer_.ReadFd(fd_, save_errno);
    if (len <= 0) {
      break;
    }
  } while (is_et_);
  return len;
}

ssize_t HttpConn::write(int *save_errno) {
  ssize_t len = 1;
  do {
    len = writev(fd_, iov_, iov_cnt_);
    if (len <= 0) {
      *save_errno = errno;
      break;
    }
    if (iov_[0].iov_len + iov_[1].iov_len == 0) {
      break;
    } else if (static_cast<size_t>(len) > iov_[0].iov_len) {
      iov_[1].iov_base = (uint8_t *)iov_[1].iov_base + (len - iov_[0].iov_len);
      iov_[1].iov_len -= (len - iov_[0].iov_len);
      if (iov_[0].iov_len) {
        write_buffer_.RetrieveAll();
        iov_[0].iov_len = 0;
      }
    } else {
      iov_[0].iov_base = (uint8_t *)iov_[0].iov_base + len;
      iov_[0].iov_len -= len;
      write_buffer_.Retrieve(len);
    }
  } while (is_et_ || toWriteBytes() > 10240);
  return len;
}

bool HttpConn::process() {
  request_.init();
  if (read_buffer_.ReadableBytes() <= 0) {
    return false;
  } else if (request_.parse(read_buffer_)) {
    LOG_DEBUG("%s", request_.path().data());
    response_.Init(src_dir_, request_.path(), request_.isKeepalive(), 200);
  } else {
    response_.Init(src_dir_, request_.path(), false, 400);
  }

  response_.MakeResponse(write_buffer_);
  // 响应头
  iov_[0].iov_base = const_cast<char *>(write_buffer_.Peek());
  iov_[0].iov_len = write_buffer_.ReadableBytes();
  iov_cnt_ = 1;

  // 文件
  if (response_.FileLen() > 0 && response_.File()) {
    iov_[1].iov_base = response_.File();
    iov_[1].iov_len = response_.FileLen();
    iov_cnt_ = 2;
  }
  LOG_DEBUG("filesize:%d, %d to %d", response_.FileLen(), iov_cnt_,
            toWriteBytes());
  return true;
}

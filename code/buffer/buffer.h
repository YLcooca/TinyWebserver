#pragma once

#include <sys/uio.h>
#include <unistd.h>

#include <atomic>
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

class Buffer {
 public:
  Buffer(const int init_size = 1024);
  ~Buffer() = default;

  size_t WriteableBytes() const;
  size_t ReadableBytes() const;
  size_t PrependableBytes() const;

  const char *Peek() const;
  void EnsureWriteable(size_t len);
  void HasWritten(size_t len);

  void Retrieve(size_t len);
  void RetrieveUntil(const char *end);
  void RetrieveAll();
  std::string RetrieveAllToStr();

  const char *BeginWriteConst() const;
  char *BeginWrite();

  void Append(const std::string &str);
  void Append(const char *str, size_t len);
  void Append(const void *data, size_t len);
  void Append(const Buffer &buff);

  ssize_t ReadFd(const int fd, int *err);
  ssize_t WriteFd(const int fd, int *err);

private:
  char *BeginPtr();
  const char *BeginPtr() const;
  void MakeResponse(size_t len);

  std::vector<char> buffer_;
  std::atomic<std::size_t> read_pos_;
  std::atomic<std::size_t> write_pos_;
};

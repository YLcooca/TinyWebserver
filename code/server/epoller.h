#pragma once
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cerrno>
#include <vector>

class Epoller {
 public:
  explicit Epoller(int maxEvent = 1024);
  ~Epoller();

  bool AddFd(const int fd, uint32_t events);
  bool ModFd(const int fd, uint32_t events);
  bool DelFd(const int fd);

  int Wait(const uint32_t timeout_ms = -1);
  int GetEventFd(size_t i) const;
  uint32_t GetEvents(size_t i) const;

 private:
  int epollfd_;
  std::vector<epoll_event> events_;
};
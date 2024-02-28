#include "epoller.h"

#include <cassert>

Epoller::Epoller(int max_event)
    : epollfd_(epoll_create(512)), events_(max_event) {
  assert(epollfd_ >= 0 && events_.size() > 0);
}

Epoller::~Epoller() { close(epollfd_); }

bool Epoller::AddFd(const int fd, uint32_t events) {
  if (fd < 1) return false;
  epoll_event ev{0};
  ev.data.fd = fd;
  ev.events = events;
  return 0 == epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &ev);
}

bool Epoller::ModFd(const int fd, uint32_t events) {
  if (fd < 1) return false;
  epoll_event ev{0};
  ev.data.fd = fd;
  ev.events = events;
  return 0 == epoll_ctl(epollfd_, EPOLL_CTL_MOD, fd, &ev);
}

bool Epoller::DelFd(const int fd) {
  if (fd < 1) return false;
  epoll_event ev{0};
  return 0 == epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, &ev);
}

int Epoller::Wait(const uint32_t timeout_ms) {
  return epoll_wait(epollfd_, &events_[0], static_cast<int>(events_.size()),
                    timeout_ms);
}

int Epoller::GetEventFd(size_t i) const {
  assert(i < events_.size() && i >= 0);
  return events_[i].data.fd;
}

uint32_t Epoller::GetEvents(size_t i) const {
  assert(i < events_.size() && i >= 0);
  return events_[i].events;
}
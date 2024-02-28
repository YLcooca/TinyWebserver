#pragma once

#include <arpa/inet.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <ctime>
#include <functional>
#include <unordered_map>
#include <vector>

#include "../log/log.h"

using TimeoutCallback = std::function<void()>;
using Clock = std::chrono::high_resolution_clock;
using MS = std::chrono::milliseconds;
using TimeStamp = Clock::time_point;

struct TimerNode {
  int id_;
  TimeStamp expires_;
  TimeoutCallback cb_;
  bool operator<(const TimerNode &rhs) { return expires_ < rhs.expires_; }
};

class HeapTimer {
 public:
  HeapTimer() { heap_.reserve(64); }
  ~HeapTimer() { clear(); }

  void adjust(int id, int expires);
  void add(int id, int timeout, const TimeoutCallback &cb);
  void doWork(int id);
  void clear();
  void tick();
  void pop();
  int getNextTick();

 private:
  std::vector<TimerNode> heap_;
  std::unordered_map<int, size_t> ref_;

  void del(size_t i);
  void siftup(size_t i);
  bool siftdown(size_t index, size_t n);
  void swap(size_t i, size_t j);
};

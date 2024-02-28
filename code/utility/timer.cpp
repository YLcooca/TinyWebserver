#include "timer.h"

void HeapTimer::siftup(size_t i) {
  assert(i >= 0 && i < heap_.size());
  size_t j = (i - 1) / 2;
  while (j >= 0) {
    if (heap_[j] < heap_[i]) {
      break;
    }
    swap(i, j);
    i = j;
    j = (i - 1) / 2;
  }
}

void HeapTimer::swap(size_t i, size_t j) {
  assert(i >= 0 && i < heap_.size());
  assert(j >= 0 && j < heap_.size());
  std::swap(heap_[i], heap_[j]);
  ref_[heap_[i].id_] = i;
  ref_[heap_[j].id_] = j;
}

bool HeapTimer::siftdown(size_t index, size_t n) {
  size_t i = index;
  size_t j = i * 2 + 1;
  while (j < n) {
    if (j + 1 < n && heap_[j + 1] < heap_[j]) ++j;
    if (heap_[i] < heap_[j]) break;
    swap(i, j);
    i = j;
    j = i * 2 + 1;
  }
  return i > index;
}

void HeapTimer::add(int id, int timeout, const TimeoutCallback &cb) {
  assert(id >= 0);
  size_t i;
  if (ref_.count(id) == 0) {
    // 新节点，插入尾部
    i = heap_.size();
    ref_[id] = i;
    heap_.push_back({id, Clock::now() + MS(timeout), cb});
    siftup(i);
  } else {
    // 已有节点，调整
    i = ref_[id];
    heap_[i].expires_ = Clock::now() + MS(timeout);
    heap_[i].cb_ = cb;
    if (!siftdown(i, heap_.size())) {
      siftup(i);
    }
  }
}

void HeapTimer::doWork(int id) {
  // 删除节点并执行回调函数
  if (heap_.empty() || ref_.count(id) == 0) {
    return;
  }
  size_t i = ref_[id];
  TimerNode node = heap_[i];
  node.cb_();
  del(i);
}

void HeapTimer::del(size_t index) {
  assert(!heap_.empty() && index >= 0 && index < heap_.size());
  // 将要删除的结点放入队尾，然后调整堆
  size_t i = index;
  size_t n = heap_.size() - 1;
  assert(i <= n);
  if (i < n) {
    swap(i, n);
    if (!siftdown(i, n)) {
      siftup(i);
    }
  }
  // 删除队尾元素
  ref_.erase(heap_.back().id_);
  heap_.pop_back();
}

void HeapTimer::adjust(int id, int timeout) {
  assert(!heap_.empty() && ref_.count(id) > 0);
  heap_[ref_[id]].expires_ = Clock::now() + MS(timeout);
  siftdown(ref_[id], heap_.size());
}

void HeapTimer::tick() {
  // 清除超时结点
  if (heap_.empty()) {
    return;
  }
  while (!heap_.empty()) {
    TimerNode node = heap_.front();
    if (std::chrono::duration_cast<MS>(node.expires_ - Clock::now()).count() >
        0) {
      break;
    }
    node.cb_();
    pop();
  }
}

void HeapTimer::pop() { del(0); }

void HeapTimer::clear() {
  ref_.clear();
  heap_.clear();
}

int HeapTimer::getNextTick() {
  tick();
  size_t res = -1;
  if (!heap_.empty()) {
    res = std::chrono::duration_cast<MS>(heap_.front().expires_ - Clock::now())
              .count();
    if (res < 0) {
      res = 0;
    }
  }
  return res;
}

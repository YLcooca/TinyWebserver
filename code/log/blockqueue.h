#pragma once
#include <cassert>
#include <condition_variable>
#include <deque>
#include <mutex>

template <typename T>
class BlockDeque {
 public:
  explicit BlockDeque(size_t max_capacity = 1000);
  ~BlockDeque();

  void clear();
  bool empty();
  bool full();
  void close();
  size_t size();
  size_t capacity();

  T front();
  T back();
  void push_back(const T &item);
  void push_front(const T &item);

  bool pop(T &item);
  bool pop(T &item, int timeout);
  void flush();

 private:
  std::deque<T> deq_;
  size_t capacity_;
  std::mutex mtx_;
  bool is_close_;
  std::condition_variable consumer_cv_;
  std::condition_variable producer_cv_;
};

template <typename T>
BlockDeque<T>::BlockDeque(size_t max_capacity) : capacity_(max_capacity) {
  assert(max_capacity > 0);
  is_close_ = false;
}

template <typename T>
BlockDeque<T>::~BlockDeque() {
  close();
}

template <typename T>
void BlockDeque<T>::close() {
  std::unique_lock locker(mtx_);
  deq_.clear();
  is_close_ = true;
  locker.unlock();
  producer_cv_.notify_all();
  consumer_cv_.notify_all();
}

template <typename T>
void BlockDeque<T>::flush() {
  consumer_cv_.notify_one();
}

template <typename T>
void BlockDeque<T>::clear() {
  std::unique_lock locker(mtx_);
  deq_.clear();
}

template <typename T>
T BlockDeque<T>::front() {
  std::unique_lock locker(mtx_);
  return deq_.front();
}

template <typename T>
T BlockDeque<T>::back() {
  std::unique_lock locker(mtx_);
  return deq_.back();
}

template <typename T>
size_t BlockDeque<T>::size() {
  std::unique_lock locker(mtx_);
  return deq_.size();
}

template <typename T>
size_t BlockDeque<T>::capacity() {
  std::unique_lock locker(mtx_);
  return capacity_;
}

template <typename T>
void BlockDeque<T>::push_back(const T &item) {
  std::unique_lock locker(mtx_);
  while (deq_.size() >= capacity_) {
    producer_cv_.wait(locker);
  }
  deq_.push_back(item);
  consumer_cv_.notify_one();
}

template <typename T>
void BlockDeque<T>::push_front(const T &item) {
  std::unique_lock locker(mtx_);
  while (deq_.size() >= capacity_) {
    producer_cv_.wait(locker);
  }
  deq_.push_front(item);
  consumer_cv_.notify_one();
}

template <typename T>
bool BlockDeque<T>::empty() {
  std::unique_lock locker(mtx_);
  return deq_.empty();
}

template <typename T>
bool BlockDeque<T>::full() {
  std::unique_lock locker(mtx_);
  return deq_.size() >= capacity_;
}

template <typename T>
bool BlockDeque<T>::pop(T &item) {
  std::unique_lock locker(mtx_);
  while (deq_.empty()) {
    consumer_cv_.wait(locker);
    if (is_close_) {
      return false;
    }
  }
  item = deq_.front();
  deq_.pop_front();
  producer_cv_.notify_one();
  return true;
}

template <typename T>
bool BlockDeque<T>::pop(T &item, int timeout) {
  std::unique_lock locker(mtx_);
  while (deq_.empty()) {
    if (consumer_cv_.wait_for(locker, std::chrono::seconds(timeout)) ==
        std::cv_status::timeout) {
      return false;
    }
    if (is_close_) {
      return false;
    }
  }
  item = deq_.front();
  deq_.pop_front();
  producer_cv_.notify_one();
  return true;
}

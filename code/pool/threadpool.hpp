#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#define cyber_unlikely(x) (x)
constexpr auto CACHELINE_SIZE = 64;

// 线程池中的等待策略
class WaitStrategy {
 public:
  virtual void NotifyOne() {}
  virtual void BreakAllWait() {}
  virtual bool EmptyWait() = 0;
  virtual ~WaitStrategy() {}
};

// block策略，最常用
class BlockWaitStrategy : public WaitStrategy {
 public:
  BlockWaitStrategy() = default;
  void NotifyOne() override { cv_.notify_one(); }
  bool EmptyWait() override {
    std::unique_lock lck(mutex_);
    cv_.wait(lck);
    return false;
  }
  void BreakAllWait() override { cv_.notify_all(); }

 private:
  std::mutex mutex_;
  std::condition_variable cv_;
};

// sleep wait
class SleepWaitStrategy : public WaitStrategy {
 public:
  SleepWaitStrategy() = default;
  explicit SleepWaitStrategy(uint64_t us) : sleep_time_us_(us) {}

  bool EmptyWait() override {
    std::this_thread::sleep_for(std::chrono::microseconds(sleep_time_us_));
    return true;
  }
  void SetSleepTimeMicroSeconds(uint64_t us) { sleep_time_us_ = us; }

 private:
  uint64_t sleep_time_us_{10000};
};

// yield wait
class YieldWaitStrategy : public WaitStrategy {
 public:
  YieldWaitStrategy() = default;
  bool EmptyWait() override {
    // 相当于被其他任务抢占了，被抢占线程会被OS重新调度
    std::this_thread::yield();
    return true;
  }
};

// timeout wait
class TimeoutBlockWaitStrategy : public WaitStrategy {
 public:
  TimeoutBlockWaitStrategy() = default;
  explicit TimeoutBlockWaitStrategy(uint64_t timeout)
      : timeout_(std::chrono::milliseconds(timeout)) {}

  void NotifyOne() override { cv_.notify_one(); }
  bool EmptyWait() override {
    std::unique_lock lck(mutex_);
    if (cv_.wait_for(lck, timeout_) == std::cv_status::timeout) {
      return false;
    }
    return true;
  }
  void BreakAllWait() override { cv_.notify_all(); }
  void SetTimeout(uint64_t timeout) {
    timeout_ = std::chrono::milliseconds(timeout);
  }

 private:
  std::mutex mutex_;
  std::condition_variable cv_;
  std::chrono::milliseconds timeout_;
};

/* 有界队列，用来存放线程池任务
 */
template <typename T>
class BoundQueue {
 public:
  using value_type = T;
  using size_type = uint64_t;

 public:
  BoundQueue& operator=(const BoundQueue&) = delete;
  BoundQueue(const BoundQueue&) = delete;

  BoundQueue() = default;
  ~BoundQueue();
  void BreakAllWait();

  bool Init(uint64_t size);
  bool Init(uint64_t size, WaitStrategy* strategy);

  bool Enqueue(const T& element);
  bool WaitEnqueue(const T& element);
  bool Dequeue(T* element);
  bool WaitDequeue(T* element);

  uint64_t Size() { return tail_ - head_ - 1; }
  bool Empty() { return Size() == 0; }
  void SetWaitStrategy(WaitStrategy* strategy) {
    wait_strategy_.reset(strategy);
  }

  uint64_t GetHead() { return head_.load(); }
  uint64_t GetTail() { return tail_.load(); }
  uint64_t GetCommit() { return commit_.load(); }

 private:
  uint64_t GetIndex(uint64_t num);

  // 指定内存对齐方式，提高代码性能
  alignas(CACHELINE_SIZE) std::atomic<uint64_t> head_{0};
  alignas(CACHELINE_SIZE) std::atomic<uint64_t> tail_{1};
  alignas(CACHELINE_SIZE) std::atomic<uint64_t> commit_{1};

  uint64_t pool_size{0};
  T* pool_{nullptr};
  std::unique_ptr<WaitStrategy> wait_strategy_{nullptr};
  volatile bool break_all_wait_{false};
};

template <typename T>
bool BoundQueue<T>::WaitDequeue(T* element) {
  while (!break_all_wait_) {
    if (Dequeue(element)) {
      return true;
    }

    if (wait_strategy_->EmptyWait()) {
      continue;
    }
    break;
  }
  return false;
}

// 基于原子变量的出队方式
template <typename T>
bool BoundQueue<T>::Dequeue(T* element) {
  uint64_t new_head = 0;
  uint64_t old_head = head_.load(std::memory_order_acquire);

  do {
    new_head = old_head + 1;
    // 没有数据可以取出
    if (new_head == commit_.load(std::memory_order_acquire)) {
      return false;
    }

    *element = pool_[GetIndex(new_head)];
  } while (!head_.compare_exchange_weak(old_head, new_head,
                                        std::memory_order_acq_rel,
                                        std::memory_order_relaxed));
  return true;
}

template <typename T>
bool BoundQueue<T>::WaitEnqueue(const T& element) {
  while (!break_all_wait_) {
    if (Enqueue(element)) {
      return true;
    }

    if (wait_strategy_->EmptyWait()) {
      continue;
    }
    break;
  }

  return false;
}

template <typename T>
bool BoundQueue<T>::Enqueue(const T& element) {
  uint64_t new_tail = 0;
  uint64_t old_commit = 0;
  uint64_t old_tail = tail_.load(std::memory_order_acquire);

  do {
    new_tail = old_tail + 1;
    if (GetIndex(new_tail) == GetIndex(head_.load(std::memory_order_acquire))) {
      return false;
    }
  } while (!tail_.compare_exchange_weak(old_tail, new_tail,
                                        std::memory_order_acq_rel,
                                        std::memory_order_relaxed));
  // 成功插入新任务
  pool_[GetIndex(old_tail)] = element;

  do {
    old_commit = old_tail;
  } while (cyber_unlikely(!commit_.compare_exchange_weak(
      old_commit, new_tail, std::memory_order_acq_rel,
      std::memory_order_relaxed)));
  // 通知在等待任务的线程取走任务
  wait_strategy_->NotifyOne();
  return true;
}

template <typename T>
inline uint64_t BoundQueue<T>::GetIndex(uint64_t num) {
  return num - (num / pool_size) * pool_size;
}

template <typename T>
inline bool BoundQueue<T>::Init(uint64_t size) {
  return Init(size, new SleepWaitStrategy());
}

template <typename T>
bool BoundQueue<T>::Init(uint64_t size, WaitStrategy* strategy) {
  // 保证队列两端各留一个空位
  pool_size = size + 2;
  pool_ = reinterpret_cast<T*>(std::calloc(pool_size, sizeof(T)));

  if (pool_ == nullptr) {
    return false;
  }

  for (uint64_t i = 0; i < pool_size; ++i) {
    // 初始化每一个元素
    new (&(pool_[i])) T();
  }
  wait_strategy_.reset(strategy);

  return true;
}

template <typename T>
inline void BoundQueue<T>::BreakAllWait() {
  break_all_wait_ = true;
  wait_strategy_->BreakAllWait();
}

template <typename T>
BoundQueue<T>::~BoundQueue() {
  if (wait_strategy_) {
    BreakAllWait();
  }
  if (pool_) {
    // 主动进行析构
    for (uint64_t i = 0; i < pool_size; ++i) {
      pool_[i].~T();
    }
  }
  std::free(pool_);
}

// 第三部分 线程池
class ThreadPool {
 public:
  explicit ThreadPool(std::size_t thread_num, std::size_t max_task_num = 1000)
      : stop_(false) {
    // 初始化失败抛出异常
    if (!task_queue_.Init(max_task_num, new BlockWaitStrategy())) {
      throw std::runtime_error("Task queue init failed.");
    }

    // 存放多个thread对象
    workers_.reserve(thread_num);
    for (size_t i = 0; i < thread_num; ++i) {
      workers_.emplace_back([this] {
        while (!stop_) {
          std::function<void()> task;
          if (task_queue_.WaitDequeue(&task)) {
            task();
          }
        }
      });
    }
  }

  template <typename F, typename... Args>
  auto Enqueue(F&& f, Args&&... args)
      -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;
    // 将函数f和args打包成一个package_task对象，放入任务队列中
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    // 并返回一个与任务相关联的future对象
    std::future<return_type> res = task->get_future();

    if (stop_) {
      return std::future<return_type>();
    }

    task_queue_.Enqueue([task]() { (*task)(); });
    return res;
  }

  inline ~ThreadPool() {
    if (stop_.exchange(true)) {
      return;
    }
    // 停止线程池中的所有线程
    task_queue_.BreakAllWait();
    // 并等待它们完成队列中的所有任务
    for (std::thread& worker : workers_) {
      worker.join();
    }
  }

 private:
  std::vector<std::thread> workers_;
  BoundQueue<std::function<void()>> task_queue_;
  std::atomic_bool stop_;
};

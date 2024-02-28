#include <cassert>
#include <functional>
#include <memory>

class Channel;
class Poller;

class EventLoop {
 public:
  using Function = std::function<void()>;
  // 初始化poller,event_fd,给event_fd注册到epoll中并注册事件处理回调
  EventLoop();
  ~EventLoop();

  // 开始事件循环
  // 调用该函数的线程必须是该EventLoop所在线程，也就是Loop函数不能跨线程调用
  void Loop();
  // 停止Loop
  void StopLoop();

  // 如果当前线程就是创建此EventLoop的线程，就调用callback
  // 否则就放入等待执行函数区
  void RunInLoop(Function&& func);
  // 把此函数放入等待执行区
  void QueueInLoop(Function&& func);

  // 把fd和绑定的事件注册到epoll内核事件表
  void PollerAdd(std::shared_ptr<Channel> channel, int timeout = 0);
  // 在epoll内核事件表修改fd所绑定的事件
  void PollerMod(std::shared_ptr<Channel> channel, int timeout = 0);
  // 在epoll内核事件表中删除fd及其绑定的事件
  void PollerDel(std::shared_ptr<Channel> channel);
  // 只关闭连接（此时还可以把缓冲区数据写完再关闭）
  void Shutdown(std::shared_ptr<Channel> channel);

  bool IsInLoopThread();

 private:
  // 创建eventfd，类似管道的进程简通信方式
  static int CreateEventfd();
  // eventfd的读回调函数（因为event_fd写了数据，所以触发可读事件，从event_fd读数据）
  void HandleRead();
  // eventfd的更新事件回调函数（更新监听事件）
  void HandleUpdate();
  // 异步唤醒subloop的epoll_wait（向event_fd中写入数据）
  void WakeUp();
  // 执行正在等待的函数
  void PerformPendingFunctions();

 private:
  std::shared_ptr<Poller> poller_;  // io多路复用分发器
  int eventfd_;                     // 用于异步唤醒Loop函数中的poll
  std::shared_ptr<Channel> wakeup_channel_;  // 用于异步唤醒的channel
  pid_t thread_id;                           // 线程id
  mutable locker::MutexLock mutex_;
  std::vector<Function> pending_functions_;  // 正在等待处理的函数

  bool is_stop_;                       // 是否停止事件循环
  bool is_looping_;                     // 是否正在事件循环
  bool is_event_handling_;             // 是否正在处理事件
  bool is_calling_pending_functions_; // 是否正在调用等待处理的函数
};


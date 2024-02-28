#include "eventloop.h"
#include <cassert>

void EventLoop::Loop() {
  assert(!is_looping_);
  assert(IsInLoopThread());
  is_looping_ = true;
  is_stop_ = false;

  while (!is_stop_) {
    // 1. epoll_wait阻塞等待就绪事件
    auto ready_channels = pooler_->Poll();
    is_event_handling_ = true;

    // 2. 处理每个就绪事件（不同channel绑定不同的callback）
    for (auto &channel : ready_channels) {
      channel->HandleEvents();
    }
    is_event_handling_ = false;
    // 3. 执行正在等待的函数（fd注册到epoll内核时间表）
    PerformPendingFunctions();
    // 4. 处理超时事件，到期了就从定时器小根堆中删除（定时器析构会EpollDel掉fd）
    poller_->HandleExpire();
  }

  is_looping_ = false;
}
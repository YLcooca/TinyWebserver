#include <functional>
#include <memory>

class Channel {
 public:
  using EventCallback = std::function<void()>;

  Channel();
  explicit Channel(int fd);
  ~Channel();

  // EventLoop中调用Loop开始事件循环，调用Poll得到就绪事件
  // 然后依次调用此函数处理就绪事件
  void HandleEvents();
  void HandleRead();    // 处理读事件的回调
  void HandleWrite();   // 处理写事件的回调
  void HandleUpdate();  // 处理更新事件的回调
  void HandleError();   // 处理错误事件的回调

  int GetFd();
  void SetFd(int fd);

  // 使用weak_ptr所指向的shared_ptr对象
  std::shared_ptr<http::HttpConnection> holder();
  void SetHolder(std::shared_ptr<http::HttpConnection> hodler);

  // 设置回调函数
  void SetReadHandler(EventCallback&& read_handler);
  void SetWriteHandler(EventCallback&& write_handler);
  void SetUpdateHandler(EventCallback&& update_handler);
  void SetErrorHandler(EventCallback&& error_handler);

  void SetRevents(int revents);
  int& GetEvents();
  void SetEvents(int events);
  int LastEvents();
  bool UpdateLastEvents();

 private:
  int fd_;
  int events_;
  int revents_;
  int last_events_;

  std::weak_ptr<http::HttpConnection> holder_;

  EventCallback read_handler_;
  EventCallback write_handler_;
  EventCallback update_handler_;
  EventCallback error_handler_;
};
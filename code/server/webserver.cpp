#include "webserver.h"

WebServer::WebServer(int port, int trig_mode, int timeout, bool opt_linger,
                     int sql_port, const char *sql_user, const char *sql_pwd,
                     const char *dbname, int connpool_num, int thread_num,
                     bool openlog, int log_level, int log_queue_size)
    : port_(port) {
  epoller_ = std::make_unique<Epoller>();
  timer_ = std::make_unique<HeapTimer>();
  threadpool_ = std::make_unique<ThreadPool>(thread_num);

  src_dir_ = getcwd(nullptr, 256);
  assert(src_dir_);
  strncat(src_dir_, "/resources/", 16);
  HttpConn::user_count_ = 0;
  HttpConn::src_dir_ = src_dir_;
  SqlConnPool::instance()->init("localhost", sql_port, sql_user, sql_pwd,
                                dbname, connpool_num);
  initEventMode(trig_mode);
  if (!initSocket()) {
    is_close_ = true;
  }

  if (openlog) {
    Log::instance()->init(log_level, "./log", ".log", log_queue_size);
    if (is_close_) {
      LOG_ERROR("========== Server init error!===========");
    } else {
      LOG_INFO("=========== Server init ===========");
      LOG_INFO("Port:%d, OpenLinger: %s", port_, opt_linger ? "true" : "false");
      LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
               (listen_event_ & EPOLLET ? "ET" : "LT"),
               (conn_event_ & EPOLLET ? "ET" : "LT"));
      LOG_INFO("LogSys level: %d", log_level);
      LOG_INFO("srcDir: %s", HttpConn::src_dir_);
      LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connpool_num,
               thread_num);
    }
  }
}

WebServer::~WebServer() {
  close(listen_fd_);
  is_close_ = true;
  free(src_dir_);
  SqlConnPool::instance()->closePool();
}

void WebServer::initEventMode(int trig_mode) {
  listen_event_ = EPOLLRDHUP;
  conn_event_ = EPOLLONESHOT | EPOLLRDHUP;
  switch (trig_mode) {
    case 0:
      break;
    case 1:
      conn_event_ |= EPOLLET;
      break;
    case 2:
      listen_event_ |= EPOLLET;
      break;
    case 3:
      listen_event_ |= EPOLLET;
      conn_event_ |= EPOLLET;
      break;
    default:
      listen_event_ |= EPOLLET;
      conn_event_ |= EPOLLET;
      break;
  }
  HttpConn::is_et_ = (conn_event_ & EPOLLET);
}

void WebServer::start() {
  int timeout = -1;
  if (!is_close_) {
    LOG_INFO("========== Server start ==========");
  }
  while (!is_close_) {
    if (timeout_ms_ > 0) {
      timeout = timer_->getNextTick();
    }
    int event_count = epoller_->Wait(timeout);
    for (int i = 0; i < event_count; ++i) {
      // 处理事件
      int fd = epoller_->GetEventFd(i);
      uint32_t events = epoller_->GetEvents(i);
      if (fd == listen_fd_) {
        dealListen();
      } else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
        assert(users_.count(fd) > 0);
        closeConn(&users_[fd]);
      } else if (events & EPOLLIN) {
        assert(users_.count(fd) > 0);
        dealRead(&users_[fd]);
      } else if (events & EPOLLOUT) {
        assert(users_.count(fd) > 0);
        dealWrtie(&users_[fd]);
      } else {
        LOG_ERROR("Unexpected event");
      }
    }
  }
}

void WebServer::sendError(int fd, const char *info) {
  assert(fd > 0);
  int ret = send(fd, info, strlen(info), 0);
  if (ret < 0) {
    LOG_WARN("send error to client[%d] error!", fd);
  }
  close(fd);
}

void WebServer::closeConn(HttpConn *client) {
  assert(client);
  LOG_INFO("client[%d] quit!", client->getFd());
  epoller_->DelFd(client->getFd());
  client->closeConn();
}

void WebServer::addClient(int fd, sockaddr_in addr) {
  assert(fd > 0);
  users_[fd].init(fd, addr);
  if (timeout_ms_ > 0) {
    timer_->add(fd, timeout_ms_,
                std::bind(&WebServer::closeConn, this, &users_[fd]));
  }
  epoller_->AddFd(fd, EPOLLIN | conn_event_);
  setFdNonblock(fd);
  LOG_INFO("client[%d] in!", users_[fd].getFd());
}

void WebServer::dealListen() {
  sockaddr_in addr;
  socklen_t len = sizeof(addr);
  do {
    int fd = accept(listen_fd_, (sockaddr *)&addr, &len);
    if (fd <= 0) {
      return;
    } else if (HttpConn::user_count_ >= MAX_FD) {
      sendError(fd, "Server busy!");
      LOG_WARN("client is full!");
      return;
    }
    addClient(fd, addr);

  } while (listen_event_ & EPOLLET);
}

void WebServer::dealRead(HttpConn *client) {
  assert(client);
  extentTime(client);
  threadpool_->Enqueue(std::bind(&WebServer::onRead, this, client));
}

void WebServer::dealWrtie(HttpConn *client) {
  assert(client);
  extentTime(client);
  threadpool_->Enqueue(std::bind(&WebServer::onWrite, this, client));
}

void WebServer::extentTime(HttpConn *client) {
  assert(client);
  if (timeout_ms_ > 0) {
    timer_->adjust(client->getFd(), timeout_ms_);
  }
}

void WebServer::onRead(HttpConn *client) {
  assert(client);
  int ret = -1;
  int read_errno = 0;
  ret = client->read(&read_errno);
  if (ret <= 0 && read_errno != EAGAIN) {
    closeConn(client);
    return;
  }
  onProcess(client);
}

void WebServer::onProcess(HttpConn *client) {
  if (client->process()) {
    epoller_->ModFd(client->getFd(), conn_event_ | EPOLLOUT);
  } else {
    epoller_->ModFd(client->getFd(), conn_event_ | EPOLLIN);
  }
}

void WebServer::onWrite(HttpConn *client) {
  assert(client);
  int ret = -1;
  int write_errno = 0;
  ret = client->write(&write_errno);
  if (client->toWriteBytes() == 0) {
    // 传输完成
    if (client->isKeepalive()) {
      onProcess(client);
      return;
    }
  } else if (ret < 0) {
    if (write_errno == EAGAIN) {
      // 继续传输
      epoller_->ModFd(client->getFd(), conn_event_ | EPOLLOUT);
      return;
    }
  }
  closeConn(client);
}

// 创建listen fd
bool WebServer::initSocket() {
  int ret = 0;
  sockaddr_in addr;
  if (port_ > 65535 || port_ < 1024) {
    LOG_ERROR("Port:%d error!", port_);
    return false;
  }
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port_);
  linger opt_linger = {0};
  if (open_linger_) {
    // 优雅关闭:直到所剩数据发生完毕或超时
    opt_linger.l_onoff = 1;
    opt_linger.l_linger = 1;
  }

  listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    LOG_ERROR("create socket error!");
    return false;
  }

  ret = setsockopt(listen_fd_, SOL_SOCKET, SO_LINGER, &opt_linger,
                   sizeof(opt_linger));
  if (ret < 0) {
    close(listen_fd_);
    LOG_ERROR("init linger error!");
    return false;
  }

  int opt_val = 1;
  // 设置端口复用
  ret = setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt_val,
                   sizeof(int));
  if (ret == -1) {
    LOG_ERROR("set socket reuseaddr error!");
    close(listen_fd_);
    return false;
  }

  ret = bind(listen_fd_, (sockaddr *)&addr, sizeof(addr));
  if (ret < 0) {
    LOG_ERROR("bind port:%d error!", port_);
    close(listen_fd_);
    return false;
  }

  ret = listen(listen_fd_, 6);
  if (ret < 0) {
    LOG_ERROR("listen port:%d error!", port_);
    close(listen_fd_);
    return false;
  }

  ret = epoller_->AddFd(listen_fd_, listen_event_ | EPOLLIN);
  if (ret == 0) {
    LOG_ERROR("Add listen fd to epoll error!");
    close(listen_fd_);
    return false;
  }
  setFdNonblock(listen_fd_);
  LOG_INFO("Server port:%d", port_);
  return true;
}

int WebServer::setFdNonblock(int fd) {
  assert(fd > 0);
  return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

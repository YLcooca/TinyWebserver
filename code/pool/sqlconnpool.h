#pragma once

#include <mysql/mysql.h>
#include <semaphore.h>

#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "../log/log.h"

class SqlConnPool {
 public:
  static SqlConnPool *instance();
  MYSQL *getConn();
  void freeConn(MYSQL *conn);
  int getFreeConnCount();
  void init(const char *host, int port, const char *user, const char *pwd,
            const char *dbname, int conn_size = 10);
  void closePool();

 private:
  SqlConnPool() = default;
  ~SqlConnPool();

  int max_count_{0};
  int use_count_{0};
  int free_count_{0};

  std::queue<MYSQL *> conn_queue_;
  std::mutex mtx_;
  sem_t sem_id_;
};
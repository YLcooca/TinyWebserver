#include "sqlconnpool.h"

SqlConnPool::~SqlConnPool() { closePool(); }

SqlConnPool *SqlConnPool::instance() {
  static SqlConnPool pool;
  return &pool;
}

void SqlConnPool::init(const char *host, int port, const char *user,
                       const char *pwd, const char *dbname, int conn_size) {
  assert(conn_size > 0);
  mysql_server_init(0, nullptr, nullptr);
  for (int i = 0; i < conn_size; ++i) {
    MYSQL *sql{nullptr};
    sql = mysql_init(sql);
    if (!sql) {
      LOG_ERROR("Mysql init error!");
      assert(sql);
    }
    sql = mysql_real_connect(sql, host, user, pwd, dbname, port, nullptr, 0);
    if (!sql) {
      LOG_ERROR("Mysql Connect error!");
    }
    conn_queue_.push(sql);
  }
  max_count_ = conn_size;
  sem_init(&sem_id_, 0, max_count_);
}

MYSQL *SqlConnPool::getConn() {
  MYSQL *sql = nullptr;
  if (conn_queue_.empty()) {
    LOG_WARN("mysql connection pool is busy!");
    return nullptr;
  }
  sem_wait(&sem_id_);
  {
    std::lock_guard locker(mtx_);
    sql = conn_queue_.front();
    conn_queue_.pop();
  }
  return sql;
}

void SqlConnPool::freeConn(MYSQL *sql) {
  assert(sql);
  std::unique_lock locker(mtx_);
  conn_queue_.push(sql);
  sem_post(&sem_id_);
}

void SqlConnPool::closePool() {
  std::unique_lock locker(mtx_);
  while (!conn_queue_.empty()) {
    auto sql = conn_queue_.front();
    conn_queue_.pop();
    mysql_close(sql);
  }
  mysql_server_end();
}

int SqlConnPool::getFreeConnCount() {
  std::unique_lock locker(mtx_);
  return conn_queue_.size();
}

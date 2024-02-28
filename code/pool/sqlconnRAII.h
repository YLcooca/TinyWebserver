#pragma once
#include "sqlconnpool.h"

class SqlConn {
 public:
  SqlConn(SqlConnPool *pool) {
    assert(pool);
    sql_ = pool->getConn();
    pool_ = pool;
  }

  ~SqlConn() {
    if (sql_) {
      pool_->freeConn(sql_);
    }
  }

  MYSQL *get() {
    return sql_;
  }
 private:
  MYSQL *sql_;
  SqlConnPool *pool_;
};

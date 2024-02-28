#pragma once

#include <sys/stat.h>
#include <sys/time.h>

#include <cassert>
#include <cstdarg>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#include "../buffer/buffer.h"
#include "blockqueue.h"

class Log {
 public:
  void init(int level, const char *path = "./log", const char *suffix = ".log",
            int max_queue_capacity = 1024);
  static Log *instance();
  static void flushLogThread();

  void write(int level, const char *format, ...);
  void flush();

  int getLevel();
  void setLevel(int level);
  bool isOpen() { return is_open_; }

 private:
  Log() = default;
  ~Log();

  void appendLogLevelTitle(int level = 0);
  void asyncWrite();

 private:
  static constexpr int LOG_PATH_LEN = 256;
  static constexpr int LOG_NAME_LEN = 256;
  static constexpr int MAX_LINES = 50000;

  const char *path_;
  const char *suffix_;

  int max_lines_{0};
  int line_count_{0};
  int today_{0};
  bool is_open_{false};
  Buffer buff_;
  int level_{0};
  bool is_async_{false};

  FILE *fp_{nullptr};
  std::unique_ptr<BlockDeque<std::string>> deque_{nullptr};
  std::unique_ptr<std::thread> write_thread_{nullptr};
  std::mutex mtx_;
};

#define LOG_BASE(level, format, ...)                 \
  do {                                               \
    Log *log = Log::instance();                      \
    if (log->isOpen() && log->getLevel() <= level) { \
      log->write(level, format, ##__VA_ARGS__);     \
      log->flush();                                  \
    }                                                \
  } while (0);

#define LOG_DEBUG(format, ...)          \
  do {                                  \
    LOG_BASE(0, format, ##__VA_ARGS__) \
  } while (0);
#define LOG_INFO(format, ...)           \
  do {                                  \
    LOG_BASE(1, format, ##__VA_ARGS__) \
  } while (0);
#define LOG_WARN(format, ...)           \
  do {                                  \
    LOG_BASE(2, format, ##__VA_ARGS__) \
  } while (0);
#define LOG_ERROR(format, ...)          \
  do {                                  \
    LOG_BASE(3, format, ##__VA_ARGS__) \
  } while (0);

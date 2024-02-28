#include "log.h"

Log::~Log() {
  if (write_thread_ && write_thread_->joinable()) {
    while (!deque_->empty()) {
      deque_->flush();
    }
    deque_->close();
    write_thread_->join();
  }
  if (fp_) {
    std::unique_lock locker(mtx_);
    flush();
    fclose(fp_);
  }
}

int Log::getLevel() {
  std::unique_lock locker(mtx_);
  return level_;
}

void Log::setLevel(int level) {
  std::unique_lock locker(mtx_);
  level_ = level;
}

void Log::init(int level = 1, const char *path, const char *suffix,
               int max_queue_size) {
  is_open_ = true;
  level_ = level;

  if (max_queue_size > 0) {
    is_async_ = true;
    if (!deque_) {
      deque_ = std::make_unique<BlockDeque<std::string>>();
      write_thread_ = std::make_unique<std::thread>(flushLogThread);
    }
  } else {
    is_async_ = false;
  }

  line_count_ = 0;

  time_t timer = time(nullptr);
  struct tm *sys_time = localtime(&timer);
  struct tm t = *sys_time;
  path_ = path;
  suffix_ = suffix;
  char file_name[LOG_NAME_LEN] = {0};
  snprintf(file_name, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", path_,
           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);
  today_ = t.tm_mday;

  std::unique_lock locker(mtx_);
  buff_.RetrieveAll();
  if (fp_) {
    flush();
    fclose(fp_);
  }

  fp_ = fopen(file_name, "a");
  if (fp_ == nullptr) {
    mkdir(path_, 0777);
    fp_ = fopen(file_name, "a");
  }
  assert(fp_ != nullptr);
}

void Log::write(int level, const char *format, ...) {
  struct timeval now = {0, 0};
  gettimeofday(&now, nullptr);
  time_t sec = now.tv_sec;
  struct tm *sys_time = localtime(&sec);
  struct tm t = *sys_time;
  va_list args;

  // 日志日期与行数
  if (today_ != t.tm_mday || (line_count_ && (line_count_ % MAX_LINES == 0))) {
    char new_file[LOG_NAME_LEN] = {0};
    char tail[36] = {0};
    snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1,
             t.tm_mday);
    if (today_ != t.tm_mday) {
      snprintf(new_file, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
      today_ = t.tm_mday;
      line_count_ = 0;
    } else {
      snprintf(new_file, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail,
               (line_count_ / MAX_LINES), suffix_);
    }
    std::unique_lock locker(mtx_);
    flush();
    fclose(fp_);
    fp_ = fopen(new_file, "a");
    assert(fp_ != nullptr);
  }
  std::unique_lock locker(mtx_);
  line_count_++;
  int n =
      snprintf(buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
               t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min,
               t.tm_sec, now.tv_usec);
  buff_.HasWritten(n);
  appendLogLevelTitle(level);
  va_start(args, format);
  int m = vsnprintf(buff_.BeginWrite(), buff_.WriteableBytes(), format, args);
  va_end(args);

  buff_.HasWritten(m);
  buff_.Append("\n\0", 2);

  if (is_async_ && deque_ && !deque_->full()) {
    deque_->push_back(buff_.RetrieveAllToStr());
  } else {
    fputs(buff_.Peek(), fp_);
  }
  buff_.RetrieveAll();
}

void Log::appendLogLevelTitle(int level) {
  switch (level) {
    case 0:
      buff_.Append("[debug]: ", 9);
      break;
    case 1:
      buff_.Append("[info] : ", 9);
      break;
    case 2:
      buff_.Append("[warn] : ", 9);
      break;
    case 3:
      buff_.Append("[error]: ", 9);
      break;
    default:
      buff_.Append("[info] : ", 9);
      break;
  }
}

void Log::flush() {
  if (is_async_) {
    deque_->flush();
  }
  fflush(fp_);
}

void Log::asyncWrite() {
  std::string str = "";
  while (deque_->pop(str)) {
    std::lock_guard locker(mtx_);
    fputs(str.c_str(), fp_);
  }
}

Log *Log::instance() {
  static Log inst;
  return &inst;
}

void Log::flushLogThread() { Log::instance()->asyncWrite(); }

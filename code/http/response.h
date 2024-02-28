#pragma once

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <unordered_map>

#include "../buffer/buffer.h"
#include "../log/log.h"

class HttpResponse {
 public:
  HttpResponse() = default;
  ~HttpResponse();

  void Init(const std::string &str_dir, std::string &path,
            bool keep_alive = false, int code = -1);
  void MakeResponse(Buffer &buff);
  void UnmapFile();
  char *File();
  size_t FileLen() const;
  void ErrorContent(Buffer &buff, std::string message);
  int Code() const { return code_; }

 private:
  void AddStateLine(Buffer &buff);
  void AddHeader(Buffer &buff);
  void AddContent(Buffer &buff);
  void ErrorHtml();
  std::string GetFileType();

  int code_{1};
  bool is_keepalive_{false};
  std::string path_{""};
  std::string src_dir_{""};
  char *mm_file_{nullptr};
  struct stat mm_filestat_ {
    0
  };

  static const std::unordered_map<std::string, std::string> suffix_type_;
  static const std::unordered_map<int, std::string> code_status_;
  static const std::unordered_map<int, std::string> code_path_;
};
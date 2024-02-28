#pragma once

#include <mysql/mysql.h>

#include <cerrno>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnRAII.h"
#include "../pool/sqlconnpool.h"

class HttpRequest {
 public:
  enum class PARSE_STATE { REQUEST_LINE, HEADERS, BODY, FINISH };
  enum class HTTP_CODE {
    NO_REQUEST,
    GET_REQUEST,
    BAD_REQUEST,
    NO_RESOURCE,
    FOBRIDDENT_REQUEST,
    FILE_REQUEST,
    INIERNAL_ERROR,
    CLOSED_CONNECTION
  };

  HttpRequest() { init(); }
  ~HttpRequest() = default;

  void init();
  bool parse(Buffer &buff);

  std::string path() const;
  std::string &path();
  std::string method() const;
  std::string version() const;
  std::string getPost(const std::string &key) const;
  std::string getPost(const char *key) const;
  bool isKeepalive() const;

 private:
  bool parseRequestLine(const std::string &line);
  void parseHeader(const std::string &line);
  void parseBody(const std::string &line);

  void parsePath();
  void parsePost();
  void parseFromUrlencoded();

  static bool userVerify(const std::string &name, const std::string &pwd,
                         bool is_login);

  PARSE_STATE state_{-1};
  std::string method_{""};
  std::string path_{""};
  std::string version_{""};
  std::string body_{""};
  std::unordered_map<std::string, std::string> header_;
  std::unordered_map<std::string, std::string> post_;

  static const std::unordered_map<std::string, int> default_html_tag_;
  static const std::unordered_set<std::string> default_html_;
  static int converHex(char ch);
};

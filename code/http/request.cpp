#include "request.h"

const std::unordered_set<std::string> HttpRequest::default_html_{
    "/index", "/register", "/login", "/welcome", "/video", "picture"};

const std::unordered_map<std::string, int> HttpRequest::default_html_tag_{
    {"/register.html", 0}, {"/login.html", 1}};

void HttpRequest::init() {
  state_ = HttpRequest::PARSE_STATE::REQUEST_LINE;
  header_.clear();
  post_.clear();
}

bool HttpRequest::isKeepalive() const {
  if (header_.count("Connection") == 1) {
    return header_.find("Connection")->second == "keep-alive" &&
           version_ == "1.1";
  }
  return false;
}

bool HttpRequest::parse(Buffer &buff) {
  const char CRLF[] = "\r\n";
  if (buff.ReadableBytes() <= 0) {
    return false;
  }
  while (buff.ReadableBytes() && state_ != HttpRequest::PARSE_STATE::FINISH) {
    const char *line_end =
        std::search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
    std::string line(buff.Peek(), line_end);
    switch (state_) {
      case HttpRequest::PARSE_STATE::REQUEST_LINE:
        if (!parseRequestLine(line)) {
          return false;
        }
        parsePath();
        break;
      case HttpRequest::PARSE_STATE::HEADERS:
        parseHeader(line);
        if (buff.ReadableBytes() <= 2) {
          state_ = HttpRequest::PARSE_STATE::FINISH;
        }
        break;
      case HttpRequest::PARSE_STATE::BODY:
        parseBody(line);
        break;
      case HttpRequest::PARSE_STATE::FINISH:
        break;
      default:
        break;
    }
    if (line_end == buff.BeginWrite()) {
      break;
    }
    buff.RetrieveUntil(line_end + 2);
  }
  LOG_DEBUG("[%s], [%s], [%s]", method_.data(), path_.data(), version_.data());
  return true;
}

void HttpRequest::parsePath() {
  if (path_ == "/") {
    path_ = "/index.html";
  } else {
    for (auto &item : default_html_) {
      if (item == path_) {
        path_ += ".html";
        break;
      }
    }
  }
}

bool HttpRequest::parseRequestLine(const std::string &line) {
  std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
  std::smatch sub_match;
  if (std::regex_match(line, sub_match, patten)) {
    method_ = sub_match[1];
    path_ = sub_match[2];
    version_ = sub_match[3];
    state_ = PARSE_STATE::HEADERS;
    return true;
  }
  LOG_ERROR("RequestLine Error!");
  return false;
}

void HttpRequest::parseHeader(const std::string &line) {
  std::regex patten("^([^:]*): ?(.*)$");
  std::smatch sub_match;
  if (std::regex_match(line, sub_match, patten)) {
    header_[sub_match[1]] = sub_match[2];
  } else {
    state_ = HttpRequest::PARSE_STATE::BODY;
  }
}

void HttpRequest::parseBody(const std::string &line) {
  body_ = line;
  parsePost();
  state_ = HttpRequest::PARSE_STATE::FINISH;
  LOG_DEBUG("Body:%s, len:%d", line.data(), line.size());
}

int HttpRequest::converHex(char ch) {
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  return ch;
}

void HttpRequest::parsePost() {
  if (method_ == "POST" &&
      header_["Content-Type"] == "application/x-www-from-urlencoded") {
    parseFromUrlencoded();
    if (default_html_tag_.count(path_)) {
      int tag = default_html_tag_.find(path_)->second;
      LOG_DEBUG("Tag:%d", tag);
      if (tag == 0 || tag == 1) {
        bool is_login = (tag == 1);
        if (userVerify(post_["username"], post_["password"], is_login)) {
          path_ = "/welcome.html";
        } else {
          path_ = "/error.html";
        }
      }
    }
  }
}

void HttpRequest::parseFromUrlencoded() {
  int n = body_.size();
  if (n == 0) return;

  std::string key, value;
  int num = 0;
  int i = 0, j = 0;

  for (; i < n; ++i) {
    char ch = body_[i];
    switch (ch) {
      case '=':
        key = body_.substr(j, i - j);
        j = i + 1;
        break;
      case '+':
        body_[i] = ' ';
        break;
      case '%':
        num = converHex(body_[i + 1]) * 16 + converHex(body_[i + 2]);
        body_[i + 2] = num % 10 + '0';
        body_[i + 1] = num / 10 + '0';
        i += 2;
        break;
      case '&':
        value = body_.substr(j, i - j);
        j = i + 1;
        post_[key] = value;
        LOG_DEBUG("%s = %s", key.data(), value.data());
        break;
      default:
        break;
    }
  }
  assert(j <= i);
  if (post_.count(key) == 0 && j < i) {
    value = body_.substr(j, i - j);
    post_[key] = value;
  }
}

bool HttpRequest::userVerify(const std::string &name, const std::string &pwd,
                             bool is_login) {
  if (name == "" || pwd == "") return false;
  LOG_INFO("Verify name:%s, pwd:%s", name.data(), pwd.data());

  auto sqlconn = SqlConn(SqlConnPool::instance());
  MYSQL *sql = sqlconn.get();
  
  bool flag = false;
  // unsigned int j = 0;
  char order[256] = {0};
  // MYSQL_FIELD *fields{nullptr};
  MYSQL_RES *res{nullptr};

  if (!is_login) {
    flag = true;
  }
  snprintf(order, 256,
           "SELECT username, password FROM user WHERE username='%s' LIMIT 1",
           name.data());
  LOG_DEBUG("%s", order);

  if (mysql_query(sql, order)) {
    mysql_free_result(res);
    return false;
  }

  res = mysql_store_result(sql);
  // j = mysql_num_fields(res);
  // fields = mysql_fetch_fields(res);

  if (MYSQL_ROW row = mysql_fetch_row(res)) {
    LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
    std::string password(row[1]);
    if (is_login) {
      if (pwd == password) {
        flag = true;
      } else {
        flag = false;
        LOG_DEBUG("password error!");
      }
    } else {
      flag = false;
      LOG_DEBUG("user used!");
    }
  }
  mysql_free_result(res);

  // 注册行为且用户名未被使用
  if (!is_login && flag == true) {
    LOG_DEBUG("user(%s) register!", name.data());
    std::memset(order, 0, 256);
    snprintf(order, 256,
             "INSERT INTO user(username, password) VALUES('%s','%s')",
             name.data(), pwd.data());
    LOG_DEBUG("order: %s", order);
    if (mysql_query(sql, order)) {
      LOG_DEBUG("Insert error!");
      flag = false;
    } else {
      flag = true;
    }
  }
  SqlConnPool::instance()->freeConn(sql);
  LOG_DEBUG("user verify success!");
  return flag;
}

std::string HttpRequest::path() const { return path_; }
std::string &HttpRequest::path() { return path_; }
std::string HttpRequest::method() const { return method_; }
std::string HttpRequest::version() const { return version_; }

std::string HttpRequest::getPost(const std::string &key) const {
  assert(key != "");
  if (post_.count(key) == 1) {
    return post_.find(key)->second;
  }
  return "";
}

std::string HttpRequest::getPost(const char *key) const {
  assert(key != nullptr);
  if (post_.count(key) == 1) {
    return post_.find(key)->second;
  }
  return "";
}
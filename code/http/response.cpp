#include "response.h"
const std::unordered_map<std::string, std::string> HttpResponse::suffix_type_ =
    {
        {".html", "text/html"},
        {".xml", "text/xml"},
        {".txt", "text/plain"},
        {"css", "text/css"},
        {".js", "text/js"},
        {".xhtml", "application/xhtml+xml"},
        {".rtf", "application/rtf"},
        {".pdf", "applocation/pdf"},
        {".word", "application/word"},
        {".gz", "application/x-gzip"},
        {".tar", "application/x-tar"},
        {".png", "image/png"},
        {".gif", "image/gif"},
        {".jpg", "image/jpg"},
        {"jpeg", "image/jpeg"},
        {".au", "audio/basic"},
        {".mpeg", "videp/mpeg"},
        {".mpg", "vide/mpg"},
        {".avi", "video/x-msvideo"},
};

const std::unordered_map<int, std::string> HttpResponse::code_status_ = {
    {200, "OK"}, {400, "Bad Reques"}, {403, "Forbidden"}, {404, "Not Found"}};

const std::unordered_map<int, std::string> HttpResponse::code_path_ = {
    {400, "/400.html"}, {403, "/403.html"}, {404, "/404.html"}};

HttpResponse::~HttpResponse() { UnmapFile(); }

void HttpResponse::Init(const std::string &src_dir, std::string &path,
                        bool is_keepalive, int code) {
  assert(src_dir != "");
  if (mm_file_) {
    UnmapFile();
  }
  code_ = code;
  is_keepalive_ = is_keepalive;
  path_ = path;
  src_dir_ = src_dir;
}

void HttpResponse::MakeResponse(Buffer &buff) {
  // 判断请求资源是否存在
  if (stat((src_dir_ + path_).data(), &mm_filestat_) < 0 ||
      S_ISDIR(mm_filestat_.st_mode)) {
    code_ = 404;
  } else if (!(mm_filestat_.st_mode & S_IROTH)) {
    code_ = 403;
  } else if (code_ == -1) {
    code_ = 200;
  }
  ErrorHtml();
  AddStateLine(buff);
  AddHeader(buff);
  AddContent(buff);
}

char *HttpResponse::File() { return mm_file_; }

size_t HttpResponse::FileLen() const { return mm_filestat_.st_size; }

void HttpResponse::ErrorHtml() {
  if (code_path_.count(code_) == 1) {
    path_ = code_path_.find(code_)->second;
    stat((src_dir_ + path_).data(), &mm_filestat_);
  }
}

void HttpResponse::AddStateLine(Buffer &buff) {
  std::string status;
  if (code_status_.count(code_) == 1) {
    status = code_status_.find(code_)->second;
  } else {
    code_ = 400;
    status = code_status_.find(400)->second;
  }
  buff.Append("HTTP/1.1 " + std::to_string(code_) + " " + status + "\r\n");
}

void HttpResponse::AddHeader(Buffer &buff) {
  buff.Append("Connection: ");
  if (is_keepalive_) {
    buff.Append("keep-alive\r\n");
    buff.Append("keep-alive: max=6, timeout=120\r\n");
  } else {
    buff.Append("close\r\n");
  }
  buff.Append("Content-type: " + GetFileType() + "\r\n");
}

void HttpResponse::AddContent(Buffer &buff) {
  int srcfd = open((src_dir_ + path_).data(), O_RDONLY);
  if (srcfd < 0) {
    ErrorContent(buff, "File NotFound!");
    return;
  }
  // 将磁盘上的文件映射到内存中
  int *mm_ret =
      (int *)mmap(0, mm_filestat_.st_size, PROT_READ, MAP_PRIVATE, srcfd, 0);
  if (*mm_ret == -1) {
    ErrorContent(buff, "File NotFound!");
  }
  mm_file_ = reinterpret_cast<char *>(mm_ret);
  close(srcfd);
  buff.Append("Content-length: " + std::to_string(mm_filestat_.st_size) +
              "\r\n\r\n");
}

void HttpResponse::UnmapFile() {
  if (mm_file_) {
    munmap(mm_file_, mm_filestat_.st_size);
    mm_file_ = nullptr;
  }
}

std::string HttpResponse::GetFileType() {
  // 判断文件类型
  std::string::size_type idx = path_.find_last_of('.');
  if (idx == std::string::npos) {
    return "text/plain";
  }
  std::string suffix = path_.substr(idx);
  if (suffix_type_.count(suffix) == 1) {
    return suffix_type_.find(suffix)->second;
  }
  return "text/plain";
}

void HttpResponse::ErrorContent(Buffer &buff, std::string message) {
  std::string body;
  std::string status;

  body += "<html><title>Error</title>";
  body += "<body bgcolor=\"ffffff\">";
  if (code_status_.count(code_) == 1) {
    status = code_status_.find(code_)->second;
  } else {
    status = "Bad Request";
  }
  body += std::to_string(code_) + " : " + status + "\n";
  body += "<p>" + message + "</p>";
  body += "<hr><em>TinyWebServer</em></body></html>";

  buff.Append("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
  buff.Append(body);
}

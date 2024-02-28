#include "buffer.h"

Buffer::Buffer(const int init_size)
    : buffer_(init_size), read_pos_(0), write_pos_(0) {}

size_t Buffer::ReadableBytes() const { return write_pos_ - read_pos_; }

size_t Buffer::WriteableBytes() const { return buffer_.size() - write_pos_; }

size_t Buffer::PrependableBytes() const { return read_pos_; }

const char *Buffer::Peek() const { return BeginPtr() + read_pos_; }

void Buffer::Retrieve(size_t len) {
  assert(len <= ReadableBytes());
  read_pos_ += len;
}

void Buffer::RetrieveUntil(const char *end) {
  assert(Peek() <= end);
  Retrieve(end - Peek());
}

void Buffer::RetrieveAll() {
  memset(&buffer_[0], 0, buffer_.size());
  read_pos_ = 0;
  write_pos_ = 0;
}

std::string Buffer::RetrieveAllToStr() {
  std::string str(Peek(), ReadableBytes());
  RetrieveAll();
  return str;
}

const char *Buffer::BeginWriteConst() const { return BeginPtr() + write_pos_; }

char *Buffer::BeginWrite() { return BeginPtr() + write_pos_; }

void Buffer::HasWritten(size_t len) { write_pos_ += len; }

void Buffer::Append(const std::string &str) { Append(str.data(), str.size()); }

void Buffer::Append(const void *data, size_t len) {
  assert(data);
  Append(static_cast<const char *>(data), len);
}

void Buffer::Append(const char *str, size_t len) {
  assert(str);
  EnsureWriteable(len);
  std::copy(str, str + len, BeginWrite());
  HasWritten(len);
}

void Buffer::Append(const Buffer &buff) {
  Append(buff.Peek(), buff.ReadableBytes());
}

void Buffer::EnsureWriteable(size_t len) {
  if (WriteableBytes() < len) {
    MakeResponse(len);
  }
  assert(WriteableBytes() >= len);
}

ssize_t Buffer::ReadFd(const int fd, int *save_err) {
  char buf[65535];
  struct iovec iov[2];
  const size_t writable = WriteableBytes();
  iov[0].iov_base = BeginPtr() + write_pos_;
  iov[0].iov_len = writable;
  iov[1].iov_base = buf;
  iov[1].iov_len = sizeof(buf);

  const ssize_t len = readv(fd, iov, 2);
  if (len < 0) {
    *save_err = errno;
  } else if (static_cast<size_t>(len) <= writable) {
    write_pos_ += len;
  } else {
    write_pos_ = buffer_.size();
    Append(buf, len - writable);
  }

  return len;
}

ssize_t Buffer::WriteFd(const int fd, int *save_err) {
  size_t read_size = ReadableBytes();
  ssize_t len = write(fd, Peek(), read_size);
  if (len < 0) {
    *save_err = errno;
    return len;
  }
  read_pos_ += len;
  return len;
}

char *Buffer::BeginPtr() { return &*buffer_.begin(); }

const char *Buffer::BeginPtr() const { return &*buffer_.begin(); }

void Buffer::MakeResponse(size_t len) {
  if (WriteableBytes() + PrependableBytes() < len) {
    buffer_.resize(write_pos_ + len + 1);
  } else {
    size_t readable = ReadableBytes();
    std::copy(BeginPtr() + read_pos_, BeginPtr() + WriteableBytes(),
              BeginPtr());
    read_pos_ = 0;
    write_pos_ = read_pos_ + readable;
    assert(readable == ReadableBytes());
  }
}

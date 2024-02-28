#include <sys/epoll.h>

#include <unordered_map>
#include <vector>

#include "channel.h"

class Poller {
 public:
  Poller();
  ~Poller();

  void Add(const Channel& request);
  void Mod(const Channel& request);
  void Del(const Channel& request);
  void Poll(std::vector<Channel>& req);

 private:
  int epollfd_;
  std::vector<epoll_event> events_;
  std::unordered_map<int, Channel> channelMap_;
};
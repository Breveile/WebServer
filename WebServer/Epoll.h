// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#pragma once
#include <sys/epoll.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include "Channel.h"
#include "HttpData.h"
#include "Timer.h"
// Epoll主要用于对系统原始的epoll库进行封装，以适应于我们现在的业务逻辑，主要的功能还是创建epoll，以及一些对epoll的操作（注册、修改、删除等等）

class Epoll {
 public:
  Epoll();// 在构造函数中创建一个epoll句柄
  ~Epoll();
  void epoll_add(SP_Channel request, int timeout);// 将需要监听的文件描述符绑定到刚才创建的epoll句柄上
  void epoll_mod(SP_Channel request, int timeout);
  void epoll_del(SP_Channel request);
  std::vector<std::shared_ptr<Channel>> poll(); // 获取epoll句柄监听的活跃事件列表
  std::vector<std::shared_ptr<Channel>> getEventsRequest(int events_num);
  void add_timer(std::shared_ptr<Channel> request_data, int timeout);
  int getEpollFd() { return epollFd_; }
  void handleExpired();

 private:
  static const int MAXFDS = 100000;
  int epollFd_;
  std::vector<epoll_event> events_;
  std::shared_ptr<Channel> fd2chan_[MAXFDS];
  std::shared_ptr<HttpData> fd2http_[MAXFDS];
  TimerManager timerManager_;
};

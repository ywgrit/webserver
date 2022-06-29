// @Author Wang Xin

#pragma once
#include <memory>
#include "Channel.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"

class Server {
 public:
  Server(EventLoop *loop, int threadNum, int port);
  ~Server() {}
  EventLoop *getLoop() const { return loop_; }
  void start();
  void handNewConn();
  void handThisConn() { loop_->updatePoller(acceptChannel_); }//如果这个acceptChannel_监听的事件或者监听的文件描述符改变了，要在loop_的poller中重新注册

 private:
  EventLoop *loop_;
  int threadNum_;
  std::unique_ptr<EventLoopThreadPool> eventLoopThreadPool_;
  bool started_;
  std::shared_ptr<Channel> acceptChannel_;
  int port_;
  int listenFd_;
  static const int MAXFDS = 100000;//限制并发连接数的原因是不让服务器过载或者不让操作系统的文件描述符资源耗尽
};

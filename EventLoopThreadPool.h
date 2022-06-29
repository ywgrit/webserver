// @Author Wang Xin

#pragma once
#include <memory>
#include <vector>
#include "EventLoopThread.h"
#include "base/Logging.h"
#include "base/noncopyable.h"


class EventLoopThreadPool : noncopyable {
 public:
  EventLoopThreadPool(EventLoop* baseLoop, int numThreads);

  ~EventLoopThreadPool() { LOG << "~EventLoopThreadPool()"; }
  void start();

  EventLoop* getNextLoop();
  /*
  在Server::handNewConn()中，getNextLoop函数被用来获取下一个EventLoop,
  依次将从server::listenfd_上监听到的连接请求分发给各个EventLoop
  */

 private:
  EventLoop* baseLoop_;// baseLoop_是在创建服务器时被创建的，这是服务器的主循环，用来将到达的连接请求分配给loops_中的某一个EventLoop
  bool started_;
  int numThreads_;
  int next_;
  std::vector<std::shared_ptr<EventLoopThread>> threads_;//存放numThreads_个EventLoopThread
  std::vector<EventLoop*> loops_;//存放numThreads_个EventLoopThread的EventLoop
};

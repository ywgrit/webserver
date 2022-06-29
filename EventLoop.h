// @Author Wang Xin

#pragma once
#include <functional>
#include <memory>
#include <vector>
#include "Channel.h"
#include "Epoll.h"
#include "Util.h"
#include "base/CurrentThread.h"
#include "base/Logging.h"
#include "base/Thread.h"


#include <iostream>
using namespace std;

// EventLoop不仅包含epoll，还包含额外的执行函数
class EventLoop {
 public:
  typedef std::function<void()> Functor;
  EventLoop();
  ~EventLoop();
  void loop();
  void quit();//EventLoopThread析构时会执行loop_->quit()和thread_.join();
  // 除了EventLoop监听的事件，用户可以往EventLoop中加入额外的函数。这些函数会在某一时刻被执行
  void runInLoop(Functor&& cb);
  /*
  由于线程可能阻塞在epoll_wait上，可能阻塞时间很长，白白浪费时间，因此可以唤醒线程，
  让其执行用户的任务，将这个任务以Functor的形式传入runInLoop中，在runInLoop中执行用户的额外任务
  用户只需将Functor传入runInLoop中，不必考虑执行runInLoop的线程是否被阻塞住，runInLoop会考虑到这一点
  */
  void queueInLoop(Functor&& cb);
  bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }//当前运行这个EventLoop对象的loop函数的线程，必须是创建这个EventLoop对象的线程，一个线程必须和一个EventLoop一一对应
  void assertInLoopThread() { assert(isInLoopThread()); }
  void shutdown(shared_ptr<Channel> channel) { shutDownWR(channel->getFd()); }
  void removeFromPoller(shared_ptr<Channel> channel) {
    // shutDownWR(channel->getFd());
    poller_->epoll_del(channel);
  }
  void updatePoller(shared_ptr<Channel> channel, int timeout = 0) {
    poller_->epoll_mod(channel, timeout);
  }
  void addToPoller(shared_ptr<Channel> channel, int timeout = 0) {
    poller_->epoll_add(channel, timeout);
  }

 private:
  // 声明顺序 wakeupFd_ > pwakeupChannel_
  bool looping_;//是否正在运行loop
  shared_ptr<Epoll> poller_;
  int wakeupFd_;
  bool quit_;
  bool eventHandling_;//正在处理IO事件标志位
  mutable MutexLock mutex_;
  /* pendingFunctors_暴露给了其他线程，多个线程可能同时访问pendingFunctors_，因此需用mutex保护
  */
  std::vector<Functor> pendingFunctors_;
  /*
  某个线程调用另一个线程的EventLoop对象中的runInLoop(Functor&& cb)来执行functor cb时，不会执行这个functor cb， 而是会将这个functor放入到这个EventLoop对象的pendingFunctors_中，接着唤醒这个EventLoop对象的所属线程A，
  让这个线程A执行这些functors
  */
  bool callingPendingFunctors_;//是否正在执行额外任务
  const pid_t threadId_;//EventLoop对象的所属线程的threadID，在EventLoop对象被创建的时候，
  //threadId_被赋值为创建EventLoop对象的线程的threadID，EventLoop对象的所属线程即为创建该EventLoop对象的线程
  shared_ptr<Channel> pwakeupChannel_;//pwakeupChannel_用来处理wakeupFd_上的可读事件

  // 会发送数据到wakeupfd_，所以监听wakeupfd_的EventLoop::loop->poll()函数会被唤醒
  void wakeup();
  void handleRead();
  void doPendingFunctors();
  void handleConn();
};

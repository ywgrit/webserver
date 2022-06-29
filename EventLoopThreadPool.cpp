// @Author Wang Xin

#include "EventLoopThreadPool.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, int numThreads)
    : baseLoop_(baseLoop), started_(false), numThreads_(numThreads), next_(0) {
  if (numThreads_ <= 0) {
    LOG << "numThreads_ <= 0";
    abort();//终止当前进程
  }
}

void EventLoopThreadPool::start() {
  baseLoop_->assertInLoopThread();// baseLoop_是在创建服务器时被创建的，使用baseLoop_的线程应和创建baseLoop_的线程一致
  started_ = true;
  for (int i = 0; i < numThreads_; ++i) {
    std::shared_ptr<EventLoopThread> t(new EventLoopThread());
    threads_.push_back(t);
    loops_.push_back(t->startLoop());
    /* 会运行各个EventLoopThread对应的thread，并且存放各个EventLoopThread的EventLoop， 这些EventLoop虽然什么都没有，但是已经运行起来了，即已经调用EventLoop::loop()函数了
    */
  }
}

// server会从所有EventLoop中依次循环取出某一个EventLoop，将新的连接请求分配给这个EventLoop
EventLoop *EventLoopThreadPool::getNextLoop() {
  baseLoop_->assertInLoopThread();
  assert(started_);
  EventLoop *loop = baseLoop_;// 如果loops_为空，则用baseloop_
  if (!loops_.empty()) {
    loop = loops_[next_];
    next_ = (next_ + 1) % numThreads_;
  }
  return loop;
}

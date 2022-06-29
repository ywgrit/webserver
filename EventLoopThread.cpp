// @Author Wang Xin

#include "EventLoopThread.h"
#include <functional>

EventLoopThread::EventLoopThread()
    : loop_(NULL),
      exiting_(false),
      thread_(bind(&EventLoopThread::threadFunc, this), "EventLoopThread"),
      mutex_(),
      cond_(mutex_) {}

EventLoopThread::~EventLoopThread() {
  exiting_ = true;
  if (loop_ != NULL) {
    loop_->quit();// 停止EventLoop的循环后，才能终止线程
    thread_.join();
  }
}

EventLoop* EventLoopThread::startLoop() {
  assert(!thread_.started());
  thread_.start();// 因为threadFunc()传入了thread_，start()会执行threadFunc()
  {
    MutexLockGuard lock(mutex_);//对EventLoopThread的loop_操作，需要先加锁
    // 一直等到threadFun在Thread里真正跑起来，如果立即返回的话，会返回一个空指针，这是不行的，所以要等threadFun在Thread里真正跑起来，然后返回一个loop_指针
    while (loop_ == NULL) cond_.wait();//和threadFunc里的cond_.notify()对应，
  }
  return loop_;
}

void EventLoopThread::threadFunc() {
  EventLoop loop;//线程运行一个loop，虽然现在这个loop里什么都没有

  {
    MutexLockGuard lock(mutex_);
    loop_ = &loop;
    cond_.notify();
  }

  loop.loop();//退出这个loop时，说明该终止线程了
  // assert(exiting_);
  loop_ = NULL;
}

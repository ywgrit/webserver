// @Author Wang Xin

#pragma once
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <functional>
#include <memory>
#include <string>
#include "CountDownLatch.h"
#include "noncopyable.h"

class Thread : noncopyable {
 public:
  typedef std::function<void()> ThreadFunc;
  explicit Thread(const ThreadFunc&, const std::string& name = std::string());//线程执行函数和线程的名字
  ~Thread();
  void start();
  int join();
  bool started() const { return started_; }
  pid_t tid() const { return tid_; }
  const std::string& name() const { return name_; }

 private:
  void setDefaultName();
  bool started_;// 线程是否开始运行 
  bool joined_;
  pthread_t pthreadId_;
  pid_t tid_;// 该线程的线程号，由gettid()函数得到，在start()函数中取得，因为start()函数中才开始创建线程
  ThreadFunc func_;
  std::string name_;
  CountDownLatch latch_;
};

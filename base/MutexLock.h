// @Author Wang Xin

#pragma once
#include <pthread.h>
#include <cstdio>
#include "noncopyable.h"


class MutexLock : noncopyable {
 public:
  MutexLock() { pthread_mutex_init(&mutex, NULL); }
  ~MutexLock() {
    pthread_mutex_lock(&mutex);
    // 注销互斥锁之前，需要先加锁
    pthread_mutex_destroy(&mutex);
  }
  void lock() { pthread_mutex_lock(&mutex); }
  void unlock() { pthread_mutex_unlock(&mutex); }
  pthread_mutex_t *get() { return &mutex; }

 private:
  pthread_mutex_t mutex;

  // 友元类不受访问权限影响，condition需要访问MutexLock中的mutex成员
 private:
  friend class Condition;
};

class MutexLockGuard : noncopyable {
 public:
  explicit MutexLockGuard(MutexLock &_mutex) : mutex(_mutex) { mutex.lock(); }
  ~MutexLockGuard() { mutex.unlock(); }

 private:
  MutexLock &mutex;// 注意这是一个引用，所以在构造函数的初始化列表当中mutex不是被拷贝构造，而是直接引用_mutex
};

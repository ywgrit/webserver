// @Author Wang Xin

#pragma once
#include <unistd.h>
#include <deque>
#include <memory>
#include <queue>
#include "HttpData.h"
#include "base/MutexLock.h"
#include "base/noncopyable.h"


class HttpData;

class TimerNode {
 public:
  TimerNode(std::shared_ptr<HttpData> requestData, int timeout);
  ~TimerNode();
  TimerNode(TimerNode &tn);
  void update(int timeout);
  bool isValid();
  void clearReq();
  void setDeleted() { deleted_ = true; }
  bool isDeleted() const { return deleted_; }
  size_t getExpTime() const { return expiredTime_; }

 private:
  bool deleted_;
  size_t expiredTime_;
  std::shared_ptr<HttpData> SPHttpData;
};

struct TimerCmp {
  bool operator()(std::shared_ptr<TimerNode> &a,
                  std::shared_ptr<TimerNode> &b) const {
    return a->getExpTime() > b->getExpTime();
  }
};

class TimerManager {
 public:
  TimerManager();
  ~TimerManager();
  void addTimer(std::shared_ptr<HttpData> SPHttpData, int timeout);
  void handleExpiredEvent();

 private:
  typedef std::shared_ptr<TimerNode> SPTimerNode;
  std::priority_queue<SPTimerNode, std::deque<SPTimerNode>, TimerCmp> timerNodeQueue;
  //注意这是优先队列，到期时间最短的时间结点被放在队列头，用优先队列实现小顶堆
};

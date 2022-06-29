// @Author Wang Xin

#include "Timer.h"
#include <sys/time.h>
#include <unistd.h>
#include <queue>

TimerNode::TimerNode(std::shared_ptr<HttpData> requestData, int timeout)
    : deleted_(false), SPHttpData(requestData) {
  struct timeval now;
  gettimeofday(&now, NULL);
  // 以毫秒计
  expiredTime_ =
      (((now.tv_sec % 10000) * 1000) + (now.tv_usec / 1000)) + timeout;
}

TimerNode::~TimerNode() {// TimerNode被放在优先队列中，当从优先队列中被弹出时，TimerNode就会被析构
  if (SPHttpData) SPHttpData->handleClose();
}

TimerNode::TimerNode(TimerNode &tn)
    : expiredTime_(0), SPHttpData(tn.SPHttpData) {}

void TimerNode::update(int timeout) {
  struct timeval now;
  gettimeofday(&now, NULL);
  expiredTime_ =
      (((now.tv_sec % 10000) * 1000) + (now.tv_usec / 1000)) + timeout;
}

bool TimerNode::isValid() {
  struct timeval now;
  gettimeofday(&now, NULL);
  size_t temp = (((now.tv_sec % 10000) * 1000) + (now.tv_usec / 1000));
  if (temp < expiredTime_)
    return true;
  else {
    this->setDeleted();
    return false;
  }
}

void TimerNode::clearReq() {
  SPHttpData.reset();
  /*
  reset()包含两个操作。当智能指针中有值的时候，调用reset()会使引用计数减1.当调用reset（new xxx())重新赋值时，
  智能指针首先是生成新对象，然后将就旧对象的引用计数减1（当然，如果发现引用计数为0时，则析构旧对象），然后将新对象的指针交给智能指针保管。
  在此处,httpdata有reset函数，会对默认的reset函数进行重载
  */
  this->setDeleted();
}

TimerManager::TimerManager() {}

TimerManager::~TimerManager() {}

// 注意，每个EventLoop都有一个TimerManager，里面包含了一个时间节点的小顶堆。只有TimerManager的所属线程才可能会使用这个TimerManager，所以操作TimerManager时是不用加锁的
void TimerManager::addTimer(std::shared_ptr<HttpData> SPHttpData, int timeout) {
  SPTimerNode new_node(new TimerNode(SPHttpData, timeout));
  timerNodeQueue.push(new_node);
  SPHttpData->linkTimer(new_node);
}

/* 处理逻辑是这样的~
因为(1) 优先队列不支持随机访问
(2) 即使支持，随机删除某节点后破坏了堆的结构，需要重新更新堆结构。
所以对于被置为deleted的时间节点，会延迟到它(1)超时 或
(2)它前面的节点都被删除时，它才会被删除。
一个点被置为deleted,它最迟会在TIMER_TIME_OUT时间后被删除。
这样做有两个好处：
(1) 第一个好处是不需要遍历优先队列，省时。
(2)
第二个好处是给超时时间一个容忍的时间，就是设定的超时时间是删除的下限(并不是一到超时时间就立即删除)，如果监听的请求在超时后的下一次请求中又一次出现了，
就不用再重新申请RequestData节点了，这样可以继续重复利用前面的RequestData，减少了一次delete和一次new的时间。
*/

// 在epoll::loop()函数中，每次循环做的事情：
// 1、处理就绪事件
// 2、执行pendingFuntors_队列中的额外函数
// 3、执行handleExpiredEvent()函数，从epoll的poller中删除超时的channel
void TimerManager::handleExpiredEvent() {
  // MutexLockGuard locker(lock);
  while (!timerNodeQueue.empty()) {
    SPTimerNode ptimer_now = timerNodeQueue.top();
    if (ptimer_now->isDeleted())
      timerNodeQueue.pop();
    else if (ptimer_now->isValid() == false)
      timerNodeQueue.pop();
    else
      break;
  }
}

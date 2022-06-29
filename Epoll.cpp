// @Author Wang Xin

#include "Epoll.h"
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <deque>
#include <queue>
#include "Util.h"
#include "base/Logging.h"
#include "MemoryPool.h"

#include <arpa/inet.h>
#include <iostream>
using namespace std;

const int EVENTSNUM = 4096;
const int EPOLLWAIT_TIME = 10000;

typedef shared_ptr<Channel> SP_Channel;
//MemoryPool<Channel> channelpool;

Epoll::Epoll() : epollFd_(epoll_create1(EPOLL_CLOEXEC)), events_(EVENTSNUM) {//epoll_create(int size)函数没有flag参数，自从linux2.6.8之后，size参数是被忽略的；
//epoll_create1(int flags)有flags参数，flags=0时epoll_create1和epoll_create函数效果是一样的,flags设置为EPOLL_CLOEXEC表示父进程fork出一个子进程后，子进程中执行exec系统调用时，子进程将关闭这个epollfd
  assert(epollFd_ > 0);
}
Epoll::~Epoll() {
    close(epollFd_);
}

// 注册新描述符
void Epoll::epoll_add(SP_Channel request, int timeout) {
  int fd = request->getFd();
  if (timeout > 0) {
    add_timer(request, timeout);
    fd2http_[fd] = request->getHolder();
  }
  struct epoll_event event;
  event.data.fd = fd;
  event.events = request->getEvents();

  request->EqualAndUpdateLastEvents();

  fd2chan_[fd] = request;
  if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &event) < 0) {
    perror("epoll_add error");
    fd2chan_[fd].reset();// 智能指针的引用计数减一，如果引用计数为0，那么释放对象
  }
}

// 修改描述符状态
void Epoll::epoll_mod(SP_Channel request, int timeout) {
  if (timeout > 0) add_timer(request, timeout);
  int fd = request->getFd();
  if (!request->EqualAndUpdateLastEvents()) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = request->getEvents();
    if (epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &event) < 0) {
      perror("epoll_mod error");
      fd2chan_[fd].reset();
    }
  }
}

// 从epoll中删除描述符
void Epoll::epoll_del(SP_Channel request) {
  int fd = request->getFd();
  struct epoll_event event;
  event.data.fd = fd;
  event.events = request->getLastEvents();
  // event.events = 0;
  // request->EqualAndUpdateLastEvents()
  if (epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &event) < 0) {
    perror("epoll_del error");
  }
  fd2chan_[fd].reset();
  fd2http_[fd].reset();
}

// 监听这个线程上设置的所有事件，只要有事件就绪就返回，返回所有活跃事件对应的channel。poll()会在loop函数中被调用，loop函数会调用所有channel的回调函数，所以poll()函数的作用就是监听，并处理就绪事件
std::vector<SP_Channel> Epoll::poll() {
  while (true) {
    int event_count =
        epoll_wait(epollFd_, &*events_.begin(), events_.size(), EPOLLWAIT_TIME);
    if (event_count < 0) perror("epoll wait error");
    std::vector<SP_Channel> req_data = getEventsRequest(event_count);
    if (req_data.size() > 0) return req_data;//如果等于0，则未监听到就绪事件发生，需要继续监听，不返回
  }
}

void Epoll::handleExpired() { timerManager_.handleExpiredEvent(); }

// 分发处理函数
std::vector<SP_Channel> Epoll::getEventsRequest(int events_num) {
  std::vector<SP_Channel> req_data;
  for (int i = 0; i < events_num; ++i) {
    // 获取有事件产生的描述符
    int fd = events_[i].data.fd;

    SP_Channel cur_req = fd2chan_[fd];
    //EventLoop* wxloop;
    //SP_Channel cur_req = channelpool.newElement(wxloop);
    //cur_req = fd2chan_[fd];
    if (cur_req) {
      cur_req->setRevents(events_[i].events);
      cur_req->setEvents(0);
      // 加入线程池之前将Timer和request分离
      // cur_req->seperateTimer();
      req_data.push_back(cur_req);
    } else {
      LOG << "SP cur_req is invalid";
    }
  }
  return req_data;
}

void Epoll::add_timer(SP_Channel request_data, int timeout) {
  shared_ptr<HttpData> t = request_data->getHolder();
  if (t)
    timerManager_.addTimer(t, timeout);
  else
    LOG << "timer add fail";
}

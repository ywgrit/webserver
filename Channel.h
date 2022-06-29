// @Author Wang Xin

#pragma once
#include <sys/epoll.h>
#include <sys/epoll.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include "Timer.h"

class EventLoop;
class HttpData;

class Channel {
/*
channel是reactor结构中的“事件”。每个channel对象自始至终只负责一个文件描述符(fd)的IO事件分发和处理，但它并不拥有这个fd，也不会在析构时关闭这个fd
channel会把不同的IO事件分发给不同的回调函数，例如Readcallbak,writecallback
// 这个fd可能是一个套接字、eventfd、timerfd、signalfd
*/
 private:
  typedef std::function<void()> CallBack;
  EventLoop *loop_;//channel所属的EventLoop,一个EventLoop对应多个channel
  int fd_;//对于acceptchannel，这是监听接收连接请求的fd，对于EventLoop的pwakeupchannel，这是监听唤醒事件的wakeupfd_,对于Httpdata的channel，这是监听IO请求的fd，会从fd读入客户端数据，或者将服务器信息传入fd
  __uint32_t events_;//这个channel要监听的事件，可能是多个事件的组合
  __uint32_t revents_;//这个channel监听到的事件，即活跃的事件，可能是多个事件的组合
  __uint32_t lastEvents_;//这个channel上次监听的事件

  // 方便找到上层持有该Channel的对象，即HttpData对象
  std::weak_ptr<HttpData> holder_;

 private:
  int parse_URI();
  int parse_Headers();
  int analysisRequest();

  CallBack readHandler_;
  CallBack writeHandler_;
  CallBack errorHandler_;
  CallBack connHandler_;// 该channel对应的文件描述符上的监听事件可能已经被改变了，需要重新注册该文件描述符上的事件

 public:
  Channel(EventLoop *loop);
  Channel(EventLoop *loop, int fd);
  ~Channel();
  int getFd();
  void setFd(int fd);

  void setHolder(std::shared_ptr<HttpData> holder) { holder_ = holder; }
  std::shared_ptr<HttpData> getHolder() {
    std::shared_ptr<HttpData> ret(holder_.lock());
    return ret;
  }

  // 使用右值引用，避免没必要的对象拷贝
  void setReadHandler(CallBack &&readHandler) { readHandler_ = readHandler; }
  void setWriteHandler(CallBack &&writeHandler) {
    writeHandler_ = writeHandler;
  }
  void setErrorHandler(CallBack &&errorHandler) {
    errorHandler_ = errorHandler;
  }
  void setConnHandler(CallBack &&connHandler) { connHandler_ = connHandler; }

  void handleEvents() {//根据revents_的值分别调用不同的回调
/*
EPOLLRDHUP和EPOLLHUP的区别：

EPOLLRDHUP 表示读关闭，有两种场景：
1、对端发送 FIN (对端调用close 或者 shutdown(SHUT_WR)).
2、本端调用 shutdown(SHUT_RD). 当然，关闭 SHUT_RD 的场景很少。

EPOLLHUP 表示读写都关闭。
1、本端调用shutdown(SHUT_RDWR)。 不能是close，close 之后，文件描述符已经失效。
2、本端调用 shutdown(SHUT_WR)，对端调用 shutdown(SHUT_WR)。
3、对端发送 RST.
*/
    events_ = 0;
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
      events_ = 0;
      return;
    }
    if (revents_ & EPOLLERR) {
      if (errorHandler_) errorHandler_();
      events_ = 0;
      return;
    }
    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
      handleRead();
    }
    if (revents_ & EPOLLOUT) {
      handleWrite();
    }
    handleConn();//这个channel对应的文件描述符上的监听事件可能改变了，需要重新注册。无论是可读事件还是可写事件执行完后，都需要重新注册监听事件
  }
  void handleRead();
  void handleWrite();
  void handleError(int fd, int err_num, std::string short_msg);
  void handleConn();

  void setRevents(__uint32_t ev) { revents_ = ev; }

  void setEvents(__uint32_t ev) { events_ = ev; }
  __uint32_t &getEvents() { return events_; }

  bool EqualAndUpdateLastEvents() {
    bool ret = (lastEvents_ == events_);
    lastEvents_ = events_;
    return ret;
  }

  __uint32_t getLastEvents() { return lastEvents_; }
};

typedef std::shared_ptr<Channel> SP_Channel;

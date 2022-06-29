// @Author Wang Xin

#include "Channel.h"

#include <unistd.h>
#include <cstdlib>
#include <iostream>

#include <queue>

#include "Epoll.h"
#include "EventLoop.h"
#include "Util.h"

using namespace std;

Channel::Channel(EventLoop *loop)//默认的监听事件是0
    : loop_(loop), fd_(0) , events_(0), lastEvents_(0){}

Channel::Channel(EventLoop *loop, int fd)//默认的监听事件是0
    : loop_(loop), fd_(fd), events_(0), lastEvents_(0) {}

Channel::~Channel() {
    /* loop_->poller_->epoll_del(fd, events_); */
    close(fd_);// linya这一行注释掉了
}

int Channel::getFd() { return fd_; }
void Channel::setFd(int fd) { fd_ = fd; }

void Channel::handleRead() {
  if (readHandler_) {
    readHandler_();
  }
}

void Channel::handleWrite() {
  if (writeHandler_) {
    writeHandler_();
  }
}

// 重新注册监听事件
void Channel::handleConn() {
  if (connHandler_) {
    connHandler_();
  }
}

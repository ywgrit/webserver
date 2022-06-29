// @Author Wang Xin

#include "EventLoop.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <iostream>
#include "Util.h"
#include "base/Logging.h"

using namespace std;

__thread EventLoop* t_loopInThisThread = 0;

int createEventfd() {
  int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  /*eventfd包含一个由内核维护的64位无符号整型计数器，0是计数器的初值，通过这个计数器，两个进程之间可以通过读取和写入eventfd来传递数据。
EFD_CLOEXEC表示创建子进程时，子进程不会继承这个eventfd。
EFD_NONBLOCK表示对eventfd执行read/write操作时，不会阻塞。
*/

  if (evtfd < 0) {
    LOG << "Failed in eventfd";
    abort();
  }
  return evtfd;
}

EventLoop::EventLoop()//创建了EventLoop对象的线程是IO线程，其主要功能是运行事件循环EventLoop::loop()
//EventLoop对象的生命周期通常和其所属线程一样长
    : looping_(false),
      poller_(new Epoll()),
      wakeupFd_(createEventfd()), // 因为需要被唤醒，每个EventLoop都有一个wakeupFd_，都是新建的
      quit_(false),
      eventHandling_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()),
      pwakeupChannel_(new Channel(this, wakeupFd_)) {
  if (t_loopInThisThread) {//每个线程只能有一个EventLoop对象，因此EventLoop的构造函数会检查当前线程是否已经创建了其他EventLoop对象，遇到错误就终止程序
    // LOG << "Another EventLoop " << t_loopInThisThread << " exists in this
    // thread " << threadId_;
  } else {
    t_loopInThisThread = this;//线程已经创建了EventLoop，所以要将本EventLoop赋给t_loopInThisThread
  }
  // pwakeupChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
  pwakeupChannel_->setEvents(EPOLLIN | EPOLLET);//边沿触发
  pwakeupChannel_->setReadHandler(bind(&EventLoop::handleRead, this));
  pwakeupChannel_->setConnHandler(bind(&EventLoop::handleConn, this));
  poller_->epoll_add(pwakeupChannel_, 0);
}

void EventLoop::handleConn() {
  // poller_->epoll_mod(wakeupFd_, pwakeupChannel_, (EPOLLIN | EPOLLET |
  // EPOLLONESHOT), 0);
  updatePoller(pwakeupChannel_, 0);
}

EventLoop::~EventLoop() {
  // wakeupChannel_->disableAll();
  // wakeupChannel_->remove();
  close(wakeupFd_);
  t_loopInThisThread = NULL;
}

// EventLoop可能会在两个地方被唤醒：1、线程A调用线程B的EventLoop的runInLoop函数，线程B会被唤醒；2、线程A结束线程B的EventLoop::loop()函数，线程B会被唤醒
void EventLoop::wakeup() {
  uint64_t one = 1;
  ssize_t n = writen(wakeupFd_, (char*)(&one), sizeof one);
  if (n != sizeof one) {
    LOG << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
  }
}

void EventLoop::handleRead() {
  /* epoll_wait监听到wakeupFd_上的读事件就绪后，说明有任务在唤醒本线程，必须将wakeupFd_上的数据读取完毕，
  以便以后能够再次唤醒本线程
  */
  uint64_t one = 1;
  ssize_t n = readn(wakeupFd_, &one, sizeof one);
  if (n != sizeof one) {
    LOG << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
  }
  // pwakeupChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
  pwakeupChannel_->setEvents(EPOLLIN | EPOLLET);
  /* 设置了之后，还要重新在poller中注册，故在调用handleRead()函数之后，还需调用handleConn()函数
  */
}

void EventLoop::runInLoop(Functor&& cb) {
  if (isInLoopThread())//如果当前线程（调用runInLoop的线程）是EventLoop所属线程，那么直接运行函数cb
    cb();
  else
    queueInLoop(std::move(cb));
    /*
    如果当前线程不是EventLoop所属线程，那么cb会被加入到EventLoop的pendingFunctors_队列中，这个EventLoop的所属线程会被唤醒来调用这个cb 
    */
}

void EventLoop::queueInLoop(Functor&& cb) {
  /*
  将Functor放入EventLoop的pendingFunctors_中，并在必要时唤醒pendingFunctors_的所属线程，让其执行pendingFunctors_中的functors；只要有其余任务需要处理，就必须马上执行或者从epoll_wait中唤醒，处理完IO事件后再执行任务
  */
  {
    MutexLockGuard lock(mutex_);
    pendingFunctors_.emplace_back(std::move(cb));// 注意是emplace_back，不是push_back，因为不用构造元素，只是插入元素
    /*
    用std::move，可以省去拷贝对象的时间和空间，并且有时候一个对象只能存在一个，这时候就更应该用std::move
    cb已经移动到pendingFunctors_向量中，cb已经等于nullptr，运行cb()会报错
    运行pendingFunctors_.back()()可以正常执行cb函数的功能
    */
  }

  // 如果调用queueInLoop的线程不是创建EventLoop的线程，或者创建EventLoop的线程正在执行pendingFunctors_，那么就唤醒该线程
  if (!isInLoopThread() || callingPendingFunctors_) wakeup();
}

void EventLoop::loop() {
  assert(!looping_);
  assert(isInLoopThread());//当前运行这个EventLoop对象的loop函数的线程，必须是创建这个EventLoop对象的线程，一个线程必须和一个EventLoop一一对应
  looping_ = true;
  quit_ = false;
  // LOG_TRACE << "EventLoop " << this << " start looping";
  std::vector<SP_Channel> ret;// 每个eventloop有多个channel。每次从poller里拿活跃事件，并给到channel里分发处理。
  while (!quit_) {
    // cout << "doing" << endl;
    ret.clear();
    ret = poller_->poll();//只是返回poller上发生就绪事件的fd对应的channel，并未进行相应处理,线程会阻塞在epoll系统调用上
    eventHandling_ = true;
    for (auto& it : ret) it->handleEvents();//依次调用每个channel的handleEvent()函数
    eventHandling_ = false;
    doPendingFunctors();
    poller_->handleExpired();//处理poller中长期不活跃的连接
  }
  LOG << " one EventLoop stop looping";
  looping_ = false;
}

// 每个EventLoop对象的pendingFunctors_可能会被多个线程访问，所以访问pendingFuntors_时需要先加锁
void EventLoop::doPendingFunctors() {
  std::vector<Functor> functors;
  callingPendingFunctors_ = true;

  {
    MutexLockGuard lock(mutex_);
    functors.swap(pendingFunctors_);//把回调列表swap()到局部变量functors中，减小了临界区的长度（意味着不会阻塞其他线程调用queueInLoop()）
  }

  for (size_t i = 0; i < functors.size(); ++i) functors[i]();
  callingPendingFunctors_ = false;
}

void EventLoop::quit() { //EventLoopThread析构时会执行EventLoop->quit()和thread_.join();
  quit_ = true;
  if (!isInLoopThread()) {
    /*    如果在一个线程A里调用另一个线程B创建的EventLoop的quit函数，即一个线程A要让另一个线程B退出loop循环
则要唤醒线程B，让线程B在loop的下一次循环中退出，因为loop中的循环是while(!quit_)，而quit_被设置为了true，所以loop会在下一次循环中退出
    */
    wakeup();
  }
}

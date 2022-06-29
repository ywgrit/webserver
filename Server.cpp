// @Author Wang Xin

#include "Server.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <functional>
#include "Util.h"
#include "base/Logging.h"

Server::Server(EventLoop *loop, int threadNum, int port)
    : loop_(loop),
      threadNum_(threadNum),
      eventLoopThreadPool_(new EventLoopThreadPool(loop_, threadNum)),
      started_(false),
      acceptChannel_(new Channel(loop_)),
      port_(port),
      listenFd_(socket_bind_listen(port_)) {
  acceptChannel_->setFd(listenFd_);
  handle_for_sigpipe();
  if (setSocketNonBlocking(listenFd_) < 0) {
    perror("set socket non block failed");
    abort();
  }
}

void Server::start() {
  eventLoopThreadPool_->start();
  // acceptChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
  acceptChannel_->setEvents(EPOLLIN | EPOLLET);
  acceptChannel_->setReadHandler(bind(&Server::handNewConn, this));//handNewConn是Server类的成员函数，不能直接将其赋给一个回调函数（函数指针实现），因为类的成员函数中默认带有“this”参数，而回调函数的形式为void()，故赋给函数指针时，编译器会报错，故需先绑定“this”参数
  acceptChannel_->setConnHandler(bind(&Server::handThisConn, this));
  loop_->addToPoller(acceptChannel_, 0);
  started_ = true;
}

void Server::handNewConn() {
  struct sockaddr_in client_addr;
  memset(&client_addr, 0, sizeof(struct sockaddr_in));
  socklen_t client_addr_len = sizeof(client_addr);
  int accept_fd = 0;
  while ((accept_fd = accept(listenFd_, (struct sockaddr *)&client_addr,
                             &client_addr_len)) > 0) {
    /*
    因为在listenfd_上注册的事件是边沿触发的，所以一次要读取完所有的连接请求
    */
    EventLoop *loop = eventLoopThreadPool_->getNextLoop();
    LOG << "New connection from " << inet_ntoa(client_addr.sin_addr) << ":"
        << ntohs(client_addr.sin_port);
    // cout << "new connection" << endl;
    // cout << inet_ntoa(client_addr.sin_addr) << endl;
    // cout << ntohs(client_addr.sin_port) << endl;
    /*
    // TCP的保活机制默认是关闭的
    int optval = 0;
    socklen_t len_optval = 4;
    getsockopt(accept_fd, SOL_SOCKET,  SO_KEEPALIVE, &optval, &len_optval);
    cout << "optval ==" << optval << endl;
    */
    // 限制服务器的最大并发连接数
    if (accept_fd >= MAXFDS) {
      close(accept_fd);
      continue;
    }
    // 设为非阻塞模式
    if (setSocketNonBlocking(accept_fd) < 0) {
      LOG << "Set non block failed!";
      // perror("Set non block failed!");
      return;
    }

    setSocketNodelay(accept_fd);
    /*
    enable accept_fd的TCP_NODELAY选项，禁用Nagle算法，避免连续发包出现延迟
    */
    // setSocketNoLinger(accept_fd);

    // 每一个新的连接到来时，都要创建一个新的HttpData对象
    shared_ptr<HttpData> req_info(new HttpData(loop, accept_fd));
    req_info->getChannel()->setHolder(req_info);
    loop->queueInLoop(std::bind(&HttpData::newEvent, req_info));
    /* 各个Loop对应的线程本可能阻塞在epoll_wait中，现在各个线程会立即从epoll_wait中被唤醒，在各个线程的epoller中加入监听这个accept_fd
    实现了新的连接请求到来时对各个线程的异步唤醒
    HttpData::newEvent中的this指针需要绑定req_info
    */
  }
  acceptChannel_->setEvents(EPOLLIN | EPOLLET);//listenFd_上的连接请求已经读取完毕，需要在listenFd_上重新注册读就绪事件
}

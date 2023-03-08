// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#include "EventLoop.h"
#include <sys/epoll.h>
// Linux中用于触发事件通知
#include <sys/eventfd.h>
#include <iostream>
#include "Util.h"
#include "base/Logging.h"

using namespace std;
// 被__thread修饰的变量，在多线程中不是共享的而是每个线程单独一个
// 保证了并发模型中的One loop per thread原则，即每个线程只能有一个EventLoop对象
__thread EventLoop* t_loopInThisThread = 0;

// 创建一个eventfd对象，初始化为0
int createEventfd() {
  int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0) {
    LOG << "Failed in eventfd";
    abort();
  }
  return evtfd;
}

// 定义EventLoop的构造函数，进行一些初始化工作
EventLoop::EventLoop()
    : looping_(false),
      poller_(new Epoll()),
      wakeupFd_(createEventfd()),
      quit_(false),
      eventHandling_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()),
      pwakeupChannel_(new Channel(this, wakeupFd_)) {
  if (t_loopInThisThread) {
    // LOG << "Another EventLoop " << t_loopInThisThread << " exists in this
    // thread " << threadId_;
  } else {
    t_loopInThisThread = this;
  }
  // pwakeupChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
  pwakeupChannel_->setEvents(EPOLLIN | EPOLLET);
  pwakeupChannel_->setReadHandler(bind(&EventLoop::handleRead, this));
  pwakeupChannel_->setConnHandler(bind(&EventLoop::handleConn, this));
  poller_->epoll_add(pwakeupChannel_, 0);
}

void EventLoop::handleConn() {
  // poller_->epoll_mod(wakeupFd_, pwakeupChannel_, (EPOLLIN | EPOLLET |
  // EPOLLONESHOT), 0);
  updatePoller(pwakeupChannel_, 0);
}

// 析构函数，关闭eventfd，并将当前线程中的Eventloop设为空，也是为了保证One loop per thread
EventLoop::~EventLoop() {
  // wakeupChannel_->disableAll();
  // wakeupChannel_->remove();
  close(wakeupFd_);
  t_loopInThisThread = NULL;
}

// 往创建的eventfd里写8个字节的内容
void EventLoop::wakeup() {
  uint64_t one = 1; // long int型，占8个字节
  ssize_t n = writen(wakeupFd_, (char*)(&one), sizeof one);
  if (n != sizeof one) {
    LOG << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
  }
}

// 从创建的eventfd里读8个字节的内容
void EventLoop::handleRead() {
  uint64_t one = 1;
  ssize_t n = readn(wakeupFd_, &one, sizeof one);
  if (n != sizeof one) {
    LOG << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
  }
  // pwakeupChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
  // 将epoll设置为ET模式
  pwakeupChannel_->setEvents(EPOLLIN | EPOLLET);
}

void EventLoop::runInLoop(Functor&& cb) {
  if (isInLoopThread())
    cb();
  else
    queueInLoop(std::move(cb));
}

void EventLoop::queueInLoop(Functor&& cb) {
  {
    MutexLockGuard lock(mutex_);
    pendingFunctors_.emplace_back(std::move(cb));
  }

  if (!isInLoopThread() || callingPendingFunctors_) wakeup();
}

void EventLoop::loop() {
  assert(!looping_);
  assert(isInLoopThread());
  looping_ = true;
  quit_ = false;
  // LOG_TRACE << "EventLoop " << this << " start looping";
  std::vector<SP_Channel> ret;
  while (!quit_) {
    // cout << "doing" << endl;
    ret.clear();
    ret = poller_->poll();
    eventHandling_ = true;
    for (auto& it : ret) it->handleEvents();
    eventHandling_ = false;
    doPendingFunctors();
    poller_->handleExpired();
  }
  looping_ = false;
}

void EventLoop::doPendingFunctors() {
  std::vector<Functor> functors;
  callingPendingFunctors_ = true;

  {
    MutexLockGuard lock(mutex_);
    functors.swap(pendingFunctors_);
  }

  for (size_t i = 0; i < functors.size(); ++i) functors[i]();
  callingPendingFunctors_ = false;
}

void EventLoop::quit() {
  quit_ = true;
  if (!isInLoopThread()) {
    wakeup();
  }
}

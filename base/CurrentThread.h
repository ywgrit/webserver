// @Author Wang Xin

#pragma once
#include <stdint.h>

namespace CurrentThread {//保存当前线程的各项信息
// internal
extern __thread int t_cachedTid;// 当前线程在内核中的tid
extern __thread char t_tidString[32];// tid格式化为字符串形式
extern __thread int t_tidStringLength;// tid占用几个字节
extern __thread const char* t_threadName;
void cacheTid();
inline int tid() {
  if (__builtin_expect(t_cachedTid == 0, 0)) {
    cacheTid();
  }
  return t_cachedTid;
}

inline const char* tidString()  // for logging
{
  return t_tidString;
}

inline int tidStringLength()  // for logging
{
  return t_tidStringLength;
}

inline const char* name() { return t_threadName; }
}

// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef _ASYNC_LOGGING_H_
#define _ASYNC_LOGGING_H_

// #include "blocking_queue.h"
#include "log_stream.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

class AsyncLogging {
  public:
    AsyncLogging(const string &basename, off_t rollSize, int flushInterval = 3);

    ~AsyncLogging() 
    {
        if (running_) 
        {
            stop();
        }
    }

    void append(const char *logline, int len);

    void start() 
    {
        running_ = true;
        /*开始执行日志罗盘函数吧*/
        thread_ = new std::thread(&AsyncLogging::threadFunc, this);
    }

    void stop() 
    {
        running_ = false;
        cond_.notify_all();
        thread_->join();
    }

  private:
    void threadFunc();
    /*一个kLargeBuffer大小的缓冲区*/
    typedef detail::FixedBuffer<detail::kLargeBuffer> Buffer;
    /*一个存储多个缓冲区智能指针的数组*/
    typedef std::vector<std::unique_ptr<Buffer>> BufferVector;
    /*其实就是Buffer的智能指针*/
    typedef BufferVector::value_type BufferPtr;

    const int flushInterval_;
    bool running_;
    const string basename_;
    const off_t rollSize_;
    std::thread *thread_;

    BufferPtr currentBuffer_;
    BufferPtr nextBuffer_;
    BufferVector buffers_;

    std::mutex mutex_;
    std::condition_variable cond_;
};

#endif // _ASYNC_LOGGING_H_

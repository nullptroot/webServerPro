// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef LOGSTREAM_H
#define LOGSTREAM_H

#include "noncopyable.h"
#include "string_piece.h"
#include "types.h"
#include <assert.h>
#include <string.h> // memcpy

namespace detail 
{

const int kSmallBuffer = 4000;
const int kLargeBuffer = 4000 * 1000;

template <int SIZE> 
class FixedBuffer : noncopyable 
{
  public:
    FixedBuffer() : cur_(data_) {}

    ~FixedBuffer() {}
    /*向缓冲区中添加日志内容*/
    void append(const char * /*restrict*/ buf, size_t len) 
    {
        // FIXME: append partially
        /*判断剩余的缓冲区是否可以放的下
        放不下就没有声音了好像*/
        if (implicit_cast<size_t>(avail()) > len) 
        {
            memcpy(cur_, buf, len);
            cur_ += len;
        }
    }
    /*返回缓冲区里的内容*/
    const char *data() const { return data_; }
    /*已经写了多少内容了*/
    int length() const { return static_cast<int>(cur_ - data_); }

    // write to data_ directly
    /*缓冲区当前写到的位置*/
    char *current() { return cur_; }
    /*还有多少剩余空间*/
    int avail() const { return static_cast<int>(end() - cur_); }
    /*当前指针加上len值*/
    void add(size_t len) { cur_ += len; }
    /*重新设置当前指针，相当于从缓冲区头开始写*/
    void reset() { cur_ = data_; }
    /*把缓冲区内容全部置零*/
    void bzero() { memZero(data_, sizeof data_); }
    /*将缓冲区内容转换为string类型*/
    string toString() const { return string(data_, length()); }
    /*转换为stringPiece类型  stringPiece 不保存字符串，只是保存字符串的指针
    也就是它没有数据的持有权，仅提供访问的能力*/
    StringPiece toStringPiece() const { return StringPiece(data_, length()); }

  private:
    const char *end() const { return data_ + sizeof data_; }
    char data_[SIZE];
    /*就是缓冲区指针现在已经用到哪了*/
    char *cur_;
};

} // namespace detail

class LogStream : noncopyable 
{
    typedef LogStream self;

  public:
    typedef detail::FixedBuffer<detail::kSmallBuffer> Buffer;

    self &operator<<(bool v) 
    {
        buffer_.append(v ? "1" : "0", 1);
        return *this;
    }

    self &operator<<(short);
    self &operator<<(unsigned short);
    self &operator<<(int);
    self &operator<<(unsigned int);
    self &operator<<(long);
    self &operator<<(unsigned long);
    self &operator<<(long long);
    self &operator<<(unsigned long long);

    self &operator<<(const void *);

    self &operator<<(float v) 
    {
        *this << static_cast<double>(v);
        return *this;
    }
    self &operator<<(double);
    // self& operator<<(long double);

    self &operator<<(char v) 
    {
        buffer_.append(&v, 1);
        return *this;
    }

    // self& operator<<(signed char);
    // self& operator<<(unsigned char);

    self &operator<<(const char *str) 
    {
        if (str) 
        {
            buffer_.append(str, strlen(str));
        } 
        else 
        {
            buffer_.append("(null)", 6);
        }
        return *this;
    }

    self &operator<<(const unsigned char *str) 
    {
        return operator<<(reinterpret_cast<const char *>(str));
    }

    self &operator<<(const string &v) 
    {
        buffer_.append(v.c_str(), v.size());
        return *this;
    }

    self &operator<<(const StringPiece &v) 
    {
        buffer_.append(v.data(), v.size());
        return *this;
    }

    self &operator<<(const Buffer &v) 
    {
        *this << v.toStringPiece();
        return *this;
    }

    void append(const char *data, int len) { buffer_.append(data, len); }
    const Buffer &buffer() const { return buffer_; }
    void resetBuffer() { buffer_.reset(); }

  private:
    void staticCheck();

    template <typename T> void formatInteger(T);

    Buffer buffer_;

    static const int kMaxNumericSize = 32;
};

class Fmt // : noncopyable
{
  public:
    template <typename T> Fmt(const char *fmt, T val);

    const char *data() const { return buf_; }
    int length() const { return length_; }

  private:
    char buf_[32];
    int length_;
};

inline LogStream &operator<<(LogStream &s, const Fmt &fmt) 
{
    s.append(fmt.data(), fmt.length());
    return s;
}

// Format quantity n in SI units (k, M, G, T, P, E).
// The returned string is atmost 5 characters long.
// Requires n >= 0
string formatSI(int64_t n);

// Format quantity n in IEC (binary) units (Ki, Mi, Gi, Ti, Pi, Ei).
// The returned string is atmost 6 characters long.
// Requires n >= 0
string formatIEC(int64_t n);

#endif // LOGSTREAM_H

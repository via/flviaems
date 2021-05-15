#pragma once

#include <cstdio>
#include <cstring>
#include <iostream>
#include <unistd.h>

/* Fd in/out streams loosely lifted from boost */

class fdoutbuf : public std::streambuf {
  int fd;

public:
  fdoutbuf(int _fd) : fd(_fd) {}

protected:
  virtual int overflow(int c) {
    if (c != EOF) {
      char z = c;
      if (write(fd, &z, 1) != 1) {
        return EOF;
      }
    }
    return c;
  }
  virtual std::streamsize xsputn(const char *s, std::streamsize num) {
    return write(fd, s, num);
  }
};

class fdostream : public std::ostream {
protected:
  fdoutbuf buf;

public:
  fdostream(int fd) : std::ostream(0), buf(fd) { rdbuf(&buf); }
};

class fdinbuf : public std::streambuf {
  int fd;

protected:
  static const int pbSize = 4;
  static const int bufSize = 1024;
  char buffer[bufSize + pbSize];

public:
  fdinbuf(int _fd) : fd(_fd) {
    setg(buffer + pbSize, buffer + pbSize, buffer + pbSize);
  }

protected:
  virtual int_type underflow() {

    /* is read position before end of buffer? */
    if (gptr() < egptr()) {
      return traits_type::to_int_type(*gptr());
    }

    /* process size of putback area
     * - use number of characters read
     * - but at most size of putback area
     */
    int numPutback;
    numPutback = gptr() - eback();
    if (numPutback > pbSize) {
      numPutback = pbSize;
    }

    /* copy up to pbSize characters previously read into
     * the putback area
     */
    memmove(buffer + (pbSize - numPutback), gptr() - numPutback, numPutback);

    // read at most bufSize new characters
    int num;
    num = read(fd, buffer + pbSize, bufSize);
    if (num <= 0) {
      // ERROR or EOF
      return EOF;
    }

    // reset buffer pointers
    setg(buffer + (pbSize - numPutback), buffer + pbSize,
         buffer + pbSize + num);

    return traits_type::to_int_type(*gptr());
  }
};

class fdistream : public std::istream {
protected:
  fdinbuf buf;

public:
  fdistream(int fd) : std::istream(0), buf(fd) { rdbuf(&buf); }
};

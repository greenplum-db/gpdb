#ifndef __GP_EXT_WRITER_H__
#define __GP_EXT_WRITER_H__

#include "reader_params.h"

class Writer {
   public:
    virtual ~Writer() {
    }

    virtual void open(const ReaderParams &params) = 0;

    // read() attempts to read up to count bytes into the buffer starting at
    // buffer.
    // Always return 0 if EOF, no matter how many times it's invoked. Throw exception if encounters
    // errors.
    virtual uint64_t write(char *buf, uint64_t count) = 0;

    // This should be reentrant, has no side effects when called multiple times.
    virtual void close() = 0;
};

#endif

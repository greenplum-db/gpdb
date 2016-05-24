#ifndef __GP_EXT_READER_H__
#define __GP_EXT_READER_H__

class Reader {
   public:
    virtual ~Reader() {}
    virtual bool open() = 0;
    // read() attempts to read up to count bytes into the buffer starting at
    // buf.
    virtual uint64_t read(char *buf, uint64_t count) = 0;
    virtual void close() = 0;
};

#endif

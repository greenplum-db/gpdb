#include <iostream>
#include <memory>
#include <vector>

#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "s3utils.h"

using std::cout;
using std::shared_ptr;
using std::make_shared;
using std::vector;

// According to S3, index value is 1 ~ 10000
// so 0 means initialization and -1 means finish / error here

struct UploadBuffer {
    UploadBuffer(uint64_t datasize);
    ~UploadBuffer(){};
    uint32_t index;
    DataBuffer data;  // TODO, ensure deleted automatically
};

UploadBuffer::UploadBuffer(uint64_t datasize) : data(datasize) { index = 0; }

concurrent_queue<shared_ptr<UploadBuffer> > eq;
concurrent_queue<shared_ptr<UploadBuffer> > fq;

void* worker(void* d) {
    shared_ptr<UploadBuffer> pdata;
    srand(time(NULL));
    // get data from fq
    while (1) {
        fq.deQ(pdata);

        if (pdata->index == 0) {
            // inited data??
            cout << "inited data" << std::endl;
        }

        if (pdata->index == -1) {  //  finished
            // cout<<"finished"<<std::endl;
            break;
        }

        MD5Calc m;
        m.Update(pdata->data.getdata(), pdata->data.len());
        cout << pdata->index << "\t" << m.Get() << std::endl;
        // sleep(rand()%5);

        // reset pdata
        pdata->data.reset();
        pdata->index = 0;

        // re queue to empty queue
        eq.enQ(pdata);
    }
    // cout<<"quit...\n";
}

#define NUMTHREADS 5
#define CHUCKSIZE 8 * 1024 * 1024U

#define READLEN 1024 * 1024

int main(int argc, char* argv[]) {
    vector<pthread_t> threads;
    shared_ptr<UploadBuffer> curdata;

    if (argc < 2) {
        printf("not enough args\n");
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open file fail");
        return 1;
    }

    char* localbuf = (char*)malloc(READLEN);
    int readlen = READLEN;

    for (int tnum = 0; tnum < NUMTHREADS; tnum++) threads.emplace_back();

    for (int tnum = 0; tnum < NUMTHREADS; tnum++) {
        eq.enQ(make_shared<UploadBuffer>(CHUCKSIZE));
    }

    for (int tnum = 0; tnum < NUMTHREADS; tnum++) {
        pthread_create(&threads[tnum], NULL, worker, NULL);
    }

    eq.deQ(curdata);
    // assert (curdata->index == 0)
    int count = 1;
    curdata->index = count++;

    int tmplen;

    while (1) {
        // read data from file
        readlen = read(fd, localbuf, READLEN);
        if (readlen == -1) {
            perror("read fail");
            break;
        } else if (readlen == 0) {  // EOF
            // cout<<"end of file\n";
            if (curdata->index > 0) fq.enQ(curdata);
            break;
        }

        //
        while (1) {
            tmplen = curdata->data.append(localbuf, readlen);
            if (tmplen < readlen) {  // change to next queue, enqueue
                fq.enQ(curdata);
                eq.deQ(curdata);
                curdata->index = count++;

                // assert (curdata->index == 0)
                readlen -= tmplen;
            } else {
                break;  // contineu to read
            }
        }
    }

    printf("Start teardown\n");

    // terminate thread
    for (int tnum = 0; tnum < NUMTHREADS; tnum++) {
        eq.deQ(curdata);
        curdata->index = -1;
        fq.enQ(curdata);
    }
    curdata.reset();
    for (int tnum = 0; tnum < NUMTHREADS; tnum++) {
        pthread_join(threads[tnum], NULL);
    }

    free(localbuf);
    close(fd);
    return 0;
}

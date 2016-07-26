#include "gpwriter.h"

GPWriter::GPWriter() {
}

GPWriter::~GPWriter() {
}

GPWriter *writer_init(const char *url_with_options) {
    GPWriter *gpwriter = new GPWriter();
    return gpwriter;
}

bool writer_transfer_data(GPWriter *writer, char *data_buf, int &data_len) {
    return true;
}

bool writer_cleanup(GPWriter **writer) {
    if (*writer) {
        delete *writer;
    }

    return true;
}

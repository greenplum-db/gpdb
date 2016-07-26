#ifndef INCLUDE_GPWRITER_H_
#define INCLUDE_GPWRITER_H_

class GPWriter {
   public:
    GPWriter();
    virtual ~GPWriter();
};

// Following 3 functions are invoked by s3_export(), need to be exception safe
GPWriter *writer_init(const char *url_with_options);
bool writer_transfer_data(GPWriter *writer, char *data_buf, int &data_len);
bool writer_cleanup(GPWriter **writer);

#endif /* INCLUDE_GPWRITER_H_ */

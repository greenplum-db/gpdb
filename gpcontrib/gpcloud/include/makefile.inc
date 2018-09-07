COMMON_OBJS = gpreader.o gpwriter.o s3conf.o s3utils.o s3log.o s3url.o s3http_headers.o s3interface.o s3restful_service.o s3bucket_reader.o s3common_reader.o s3common_writer.o decompress_reader.o compress_writer.o s3key_reader.o s3key_writer.o

COMMON_LINK_OPTIONS = -lstdc++ -lxml2 -lpthread -lcrypto -lcurl -lz

COMMON_CPP_FLAGS = -std=c++11 -fPIC -I/usr/include/libxml2 -I/usr/local/opt/openssl/include

TEST_OBJS = $(patsubst %.o,%_test.o,$(COMMON_OBJS))

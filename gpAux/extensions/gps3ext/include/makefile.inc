COMMON_OBJS = s3conf.o s3common.o s3utils.o s3log.o s3url_parser.o s3http_headers.o s3interface.o s3restful_service.o uncompress_reader.o s3key_reader.o s3bucket_reader.o #gpreader.o s3extbase.o

COMMON_LINK_OPTIONS = -lstdc++ -lxml2 -lpthread -lcrypto -lcurl -lz

COMMON_CPP_FLAGS = -O2 -std=c++98 -Wall -fPIC -I/usr/include/libxml2

TEST_OBJS = $(patsubst %.o,%_test.o,$(COMMON_OBJS))

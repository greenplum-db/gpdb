MODULE_big = gps3ext
OBJS = src/gps3ext.o src/S3ExtWrapper.o lib/http_parser.o src/S3Common.o src/S3Downloader.o src/utils.o src/S3Log.o src/gps3conf.o lib/ini.o

PG_CPPFLAGS = -std=c++98 -fPIC -I$(libpq_srcdir) -Isrc -Ilib -I/usr/include/libxml2 -I$(libpq_srcdir)/postgresql/server/utils -g
SHLIB_LINK = -lstdc++ -lxml2 -lpthread -lcrypto -lcurl

PGXS := $(shell pg_config --pgxs)
include $(PGXS)

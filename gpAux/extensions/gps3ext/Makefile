# Options
DEBUG_S3 = n
DEBUG_S3_CURL = n

# Flags
SHLIB_LINK = -lstdc++ -lxml2 -lpthread -lcrypto -lcurl
PG_CPPFLAGS = -O2 -g -std=c++98 -fPIC -I$(libpq_srcdir) -Isrc -Ilib -I/usr/include/libxml2 -I$(libpq_srcdir)/postgresql/server/utils

ifeq ($(DEBUG_S3),y)
	PG_CPPFLAGS += -DDEBUG_S3
endif

ifeq ($(DEBUG_S3_CURL),y)
	PG_CPPFLAGS += -DDEBUG_S3_CURL
endif

# Targets
MODULE_big = gps3ext
OBJS = lib/http_parser.o lib/ini.o src/gps3ext.o src/s3conf.o src/s3common.o src/s3wrapper.o src/s3downloader.o src/s3utils.o src/s3log.o

# Launch
PGXS := $(shell pg_config --pgxs)
include $(PGXS)

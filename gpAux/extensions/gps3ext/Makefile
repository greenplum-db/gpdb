# Options
DEBUG_S3_CURL = n
DEBUG_S3_SYMBOL = y

# Flags
SHLIB_LINK += -lstdc++ -lxml2 -lpthread -lcrypto -lcurl -lz
PG_CPPFLAGS += -O2 -std=c++98 -Wall -fPIC -Iinclude -Ilib -I/usr/include/libxml2 -I$(libpq_srcdir) -I$(libpq_srcdir)/postgresql/server/utils

ifeq ($(DEBUG_S3_CURL),y)
	PG_CPPFLAGS += -DDEBUG_S3_CURL
endif

ifeq ($(DEBUG_S3_SYMBOL),y)
	PG_CPPFLAGS += -g
endif

# Targets
MODULE_big = gps3ext
OBJS = lib/http_parser.o lib/ini.o src/gps3ext.o src/s3conf.o src/s3common.o src/gpreader.o src/s3utils.o src/s3log.o src/s3url_parser.o src/s3http_headers.o src/s3thread.o src/s3extbase.o src/s3reader.o

# Launch
PGXS := $(shell pg_config --pgxs)
include $(PGXS)

format:
	@make -f Makefile.others format

lint:
	@make -f Makefile.others lint

tags:
	@make -f Makefile.others tags

test:
	@make -f Makefile.others test

coverage:
	@make -f Makefile.others coverage

.PHONY: format lint tags test coverage

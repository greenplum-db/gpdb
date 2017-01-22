# Include
include ../../include/makefile.inc

# Options
DEBUG_S3_SYMBOL = y

CPPFLAGS = $(COMMON_CPP_FLAGS) -I../../include -I../../lib -DS3_STANDALONE -DS3_STANDALONE_CHECKCLOUD
LDFLAGS = $(COMMON_LINK_OPTIONS)

ifeq ($(DEBUG_S3_SYMBOL),y)
       CPPFLAGS += -g
endif

# Targets
PROGRAM = gpcheckcloud

%.o: ../../src/%.cpp
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

http_parser.o: ../../lib/http_parser.cpp
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

ini.o: ../../lib/ini.cpp
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

gpcheckcloud: gpcheckcloud.o http_parser.o ini.o $(COMMON_OBJS)

install:

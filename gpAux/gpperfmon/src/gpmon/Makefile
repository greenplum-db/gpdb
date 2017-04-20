SHELL=/bin/bash

default: all

top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

LIBPQ_DIR=$(top_builddir)/src/interfaces/libpq

# TODO: figure out what to do this "enterprise" variable
osx105_x86_COPT=-Wl,-search_paths_first
COPT=$($(BLD_ARCH)_COPT)
OPT=-g -O3 -Wall
ifneq ($(CUSTOM_COPT),)
COPT=$(CUSTOM_COPT)
endif

# only redhat has snmp, EMC related stuff due to DCA"
ifneq "$(findstring $(BLD_ARCH),rhel5_x86_64 rhel6_x86_64 rhel7_x86_64)" ""
CONNECTEMCLIB += -lemcconnect
endif

GPMMON_OBJS=gpmmon.o gpmondb.o gpmonlib.o gpmon_agg.o

GPSMON_OBJS=gpsmon.o gpmonlib.o
SIGTEST_OBJS=sigartest.o

INCS = -I. $(CPPFLAGS) $(apr_includes) $(apr_cppflags) $(apu_includes) -I$(LIBPQ_DIR)
override CFLAGS := $(CFLAGS) $(apu_cflags)

LDLIBS = $(LDFLAGS) $(LIBS) -levent $(apu_link_ld_libs) $(apr_link_ld_libs)

GPSMON_LIBS=
GPMMON_LIBS=-L$(LIBPQ_DIR) -lpq -lm

ifeq "$(findstring osx,$(BLD_ARCH))" "osx"
GPSMON_LIBS=-framework CoreServices -framework IOKit
SIGTEST_LIBS=-framework CoreServices -framework IOKit
endif

GPSMON_LIBS+= -lm -lsigar
SIGTEST_LIBS+= -lsigar

all: gpsmon gpmmon

gpsmon: $(GPSMON_OBJS)
	$(CC) -o gpsmon $(CFLAGS) $(COPT) $(GPSMON_OBJS) $(LDLIBS) $(GPSMON_LIBS)

gpmmon: $(GPMMON_OBJS)
	$(CC) -o gpmmon $(CFLAGS) $(COPT) $(GPMMON_OBJS) $(LDLIBS) $(GPMMON_LIBS)

sigartest: $(SIGTEST_OBJS)
	$(CC) -o sigartest $(CFLAGS) $(COPT) $(SIGTEST_OBJS) $(LDLIBS) $(GPSMON_LIBS)

%.o: %.c
ifeq ($(enable_gpperfmon),no)
	$(error cannot build without --enable-gpperfmon in global configuration)
endif
	$(CC) $(INCS) $(CPPFLAGS) $(CFLAGS) $(COPT) -o $@ -c $<

install: all
	mkdir -p $(prefix)/bin
	cp -p gpmmon $(prefix)/bin
	cp -p gpsmon $(prefix)/bin
	cp -p gpperfmoncat.sh $(prefix)/bin
	cp -p gpmon_catqrynow.py $(prefix)/sbin
	cp -p gpperfmon_install $(prefix)/bin
	mkdir -p $(prefix)/lib/gpperfmon
	cp -p gpperfmon3.sql $(prefix)/lib/gpperfmon
	cp -p gpperfmon4.sql $(prefix)/lib/gpperfmon
	cp -p gpperfmon41.sql $(prefix)/lib/gpperfmon
	cp -p gpperfmon42.sql $(prefix)/lib/gpperfmon
	cp -p gpperfmonC.sql $(prefix)/lib/gpperfmon


clean:
	rm -rf *.o gpmmon gpsmon sigartest

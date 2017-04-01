SHELL=/bin/bash

default: all

BLD_TOP=$(shell echo `pwd`/../../../)
include $(BLD_TOP)/Makefile.global
include $(BLD_TOP)/Makefile.thirdparty

EXT_DIR=$(BLD_TOP)/ext/$(BLD_ARCH)/
LIBPQ_DIR=$(BLD_TOP)/../src/interfaces/libpq/

osx105_x86_COPT=-Wl,-search_paths_first
CC=$(strip $(BLD_CC) $(BLD_CFLAGS))
COPT=$($(BLD_ARCH)_COPT)
OPT=-g -O3 -Wall
ifneq ($(CUSTOM_COPT),)
	COPT=$(CUSTOM_COPT)
endif

ifneq "$(findstring $(BLD_ARCH),rhel5_x86_64 rhel6_x86_64 rhel7_x86_64)" ""
CONNECTEMCLIB += -lemcconnect
endif

GPMMON_OBJS=gpmmon.o gpmondb.o gpmonlib.o gpmon_agg.o

GPSMON_OBJS=gpsmon.o gpmonlib.o
SIGTEST_OBJS=sigartest.o

INCS=-I$(EXT_DIR)/include -I$(EXT_DIR)/include/apr-1 -I$(BLD_TOP)/../src/include -I$(LIBPQ_DIR)
LIBS=-L$(EXT_DIR)/lib -laprutil-1 -lapr-1 -levent $$($(EXT_DIR)/bin/apu-1-config --libs) $$($(EXT_DIR)/bin/apr-1-config --libs) 
GPSMON_LIBS=
GPMMON_LIBS=-L$(LIBPQ_DIR) -lpq -lm -lsnmp $(CONNECTEMCLIB)
CFLAGS=$(OPT) $$($(EXT_DIR)/bin/apr-1-config --cflags) -Werror -Wall
CPPFLAGS=$$($(EXT_DIR)/bin/apr-1-config --cppflags) -D$(BLD_ARCH)

ifeq "$(findstring osx,$(BLD_ARCH))" "osx"
	GPSMON_LIBS=-framework CoreServices -framework IOKit
	SIGTEST_LIBS=-framework CoreServices -framework IOKit
endif

ifeq (sol,$(findstring sol,$(BLD_ARCH)))
	GPSMON_LIBS+=-lkstat
endif

GPSMON_LIBS+= -lm -lsigar
SIGTEST_LIBS+= -lsigar

all: gpsmon gpmmon

gpsmon: $(GPSMON_OBJS)
	$(CC) -o gpsmon $(COPT) $(GPSMON_OBJS) $(LIBS) $(GPSMON_LIBS)

gpmmon: $(GPMMON_OBJS)
	$(CC) -o gpmmon $(COPT) $(GPMMON_OBJS) $(LIBS) $(GPMMON_LIBS)

sigartest: $(SIGTEST_OBJS)
	$(CC) -o sigartest $(COPT) $(SIGTEST_OBJS) $(LIBS) $(GPSMON_LIBS)

%.o: %.c
	$(CC) $(INCS) $(CPPFLAGS) $(CFLAGS) $(COPT) -o $@ -c $<

install:
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

#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for utils/resgroup
#
# IDENTIFICATION
#    src/backend/utils/resgroup/Makefile
#
#-------------------------------------------------------------------------

subdir = src/backend/utils/resgroup
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global
override CPPFLAGS := -I$(libpq_srcdir) $(CPPFLAGS)

OBJS = resgroup.o resgroup_helper.o

ifeq ($(PORTNAME),linux)
OBJS += cgroup.o
OBJS += cgroup-ops-linux-v1.o
OBJS += cgroup-ops-linux-v2.o
OBJS += io_limit_scanner.o
OBJS += io_limit_gram.o
OBJS += cgroup_io_limit.o

include $(top_srcdir)/src/backend/common.mk

io_limit_gram.h: io_limit_gram.c
	touch $@

io_limit_gram.c: BISONFLAGS += -d

io_limit_scanner.c: FLEXFLAGS = -CF -p -p
io_limit_scanner.c: FLEX_NO_BACKUP=yes
io_limit_scanner.c: FLEX_FIX_WARNING=yes

io_limit_gram.o io_limit_scanner.o cgroup-ops-linux-v2.o: io_limit_gram.h
else
OBJS += cgroup-ops-dummy.o

include $(top_srcdir)/src/backend/common.mk

endif

clean distclean maintainer-clean:
	rm -f lex.backup

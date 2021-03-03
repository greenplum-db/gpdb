SUBDIRS = extension

PROGRAM = extended_protocol_commit_test

OBJS = extended_protocol_commit_test.o

REGRESS = extended_protocol_commit_test


subdir = src/test/isolation2
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global
NO_PGXS = 1
include $(top_srcdir)/src/makefiles/pgxs.mk
include $(top_srcdir)/src/Makefile.shlib

ifeq ($(PORTNAME), win32)
LDLIBS += -lws2_32
endif
override CPPFLAGS := -I$(srcdir) -I$(libpq_srcdir) -I$(srcdir)/../regress $(CPPFLAGS)
override LDLIBS := $(libpq_pgport) $(LDLIBS)


installcheck: install

extended_protocol_commit_test: extended_protocol_commit_test.c
	$(CC) $(CPPFLAGS) -I$(top_builddir)/src/interfaces/libpq -L$(top_builddir)/src/interfaces/libpq -o $@ $< -lpq


$(call recurse,all clean installcheck)

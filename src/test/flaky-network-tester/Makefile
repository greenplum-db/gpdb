#
# Makefile for flaky connection emulating socket wrapper
#

subdir = src/test/flay-network-tester
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

override CPPFLAGS := -I$(libpq_srcdir) $(CPPFLAGS)
override LDLIBS := $(libpq_pgport) $(LDLIBS)

# Build dynamically-loaded object file

NAME = flaky-wrap
OBJS = flaky-wrap.o

include $(top_srcdir)/src/Makefile.shlib

all: all-lib

clean: clean-lib
	rm -f $(OBJS)


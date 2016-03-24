#
# Makefile for example programs
#

subdir = src/test/dispatch-performance
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

override CPPFLAGS := -I$(libpq_srcdir) $(CPPFLAGS)
override LDLIBS := $(libpq_pgport) $(LDLIBS)


PROGS = dispatchtest

all: $(PROGS)

clean: 
	rm -f $(PROGS)

EXTENSION = pxf
DATA = pxf--1.0.sql
MODULE_big = pxf
OBJS       = src/pxfprotocol.o
REGRESS    = setup pxf

ifdef USE_PGXS
PGXS := $(shell pg_config --pgxs)
include $(PGXS)
else
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

unittest-check:
	$(MAKE) -C test check

unittest-clean:
	$(MAKE) -C test clean

clean: unittest-clean

.PHONY: unittest-check unittest-clean

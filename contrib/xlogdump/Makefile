VERSION_STR="0.6devel"

PROGRAM = xlogdump
OBJS    = xlogdump.o xlogdump_rmgr.o xlogdump_statement.o xlogdump_oid2name.o

PG_CPPFLAGS = -DVERSION_STR=\"$(VERSION_STR)\" -I. -I$(libpq_srcdir) -DDATADIR=\"$(datadir)\"
PG_LIBS = $(libpq_pgport)

DATA = oid2name.txt
EXTRA_CLEAN = oid2name.txt

DOCS = README.xlogdump

ifdef USE_PGXS
PGXS := $(shell pg_config --pgxs)
include $(PGXS)
else
subdir = contrib/xlogdump
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

majorversion=`echo $(PG_VERSION) | sed -e 's/^\([0-9]*\)\.\([0-9]*\).*/\1\2/g'`

xlogdump_oid2name.o: oid2name.txt

oid2name.txt:
	cp oid2name-$(majorversion).txt oid2name.txt

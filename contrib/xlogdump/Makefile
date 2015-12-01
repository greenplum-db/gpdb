# $PostgreSQL: pgsql/contrib/pgbench/Makefile,v 1.14 2005/09/27 17:13:08 tgl Exp $

PROGRAM = xlogdump
OBJS    = xlogdump.o ../../src/port/sprompt.o ../../src/backend/utils/hash/pg_crc.o

PG_CPPFLAGS = -I$(libpq_srcdir)
PG_LIBS = $(libpq_pgport) -pthread  $(libpq)
#
# Add libraries that libpq depends (or might depend) on into the
# link.  (The order in which you list them here doesn't
# matter.)
ifneq ($(PORTNAME), win32)
#PG_LIBS += -llber -lcom_err -lk5crypto $(LDAP_LIBS_FE) $(PTHREAD_LIBS) 
PG_LIBS += $(LDAP_LIBS_FE) $(PTHREAD_LIBS) 
endif


ifdef USE_PGXS
PGXS := $(shell pg_config --pgxs)
include $(PGXS)
else
subdir = contrib/xlogdump
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

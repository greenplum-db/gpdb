#-------------------------------------------------------------------------
#
# Makefile for src/bin/gpperfmon
#
# Copyright (c) 2016, Pivotal Inc.
#
# src/bin/gpperfmon/Makefile
#
#-------------------------------------------------------------------------

PGFILEDESC = "gpperfmon"

subdir = src/bin/gpperfmon
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

GPMMON_OBJS = gpmmon.o gpmondb.o gpmonlib.o gpmon_agg.o
GPSMON_OBJS = gpsmon.o gpmonlib.o

ifneq ($(enable_snmp), no)
override LDFLAGS += -lnetsnmp
GPMMON_OBJS += gpmon_snmp.o
endif

all: gpmmon gpsmon

CFLAGS += $(apr_cflags) $(apr_includes) -I$(libpq_srcdir)
LDFLAGS +=  -levent

gpmmon: $(GPMMON_OBJS)
	$(CC) $(CFLAGS) $(GPMMON_OBJS) $(libpq_pgport) $(apu_link_ld_libs) $(apr_link_ld_libs) -levent $(LDFLAGS) -o $@$(X)

gpsmon: $(GPSMON_OBJS)
	$(CC) $(CFLAGS) $(GPSMON_OBJS) $(libpq_pgport) $(apu_link_ld_libs) $(apr_link_ld_libs) -lsigar -lm $(LDFLAGS) -o $@$(X)

snmptest: snmptest.o
	$(CC) $(CFLAGS) snmptest.o -o $@$(X) -lnetsnmp

sigartest: sigartest.o
	$(CC) $(CFLAGS) sigartest.o -o $@$(X) -lsigar

install: all installdirs
	$(INSTALL_PROGRAM) gpmmon$(X) '$(DESTDIR)$(bindir)/gpmmon$(X)'
	$(INSTALL_PROGRAM) gpsmon$(X) '$(DESTDIR)$(bindir)/gpsmon$(X)'
	$(INSTALL_PROGRAM) gpperfmon '$(DESTDIR)$(bindir)/gpperfmon'
	$(INSTALL_PROGRAM) gpperfmon.py '$(DESTDIR)$(bindir)/gpperfmon.py'
	$(INSTALL_PROGRAM) gpmon_catqrynow.py '$(DESTDIR)$(bindir)/gpmon_catqrynow.py'
	$(INSTALL_PROGRAM) gpperfmoncat.sh '$(DESTDIR)$(bindir)/gpperfmoncat.sh'
	$(INSTALL_DATA) gpperfmon3.sql '$(DESTDIR)$(libdir)a/gpperfmon/gpperfmon3.sql'
	$(INSTALL_DATA) gpperfmon4.sql '$(DESTDIR)$(libdir)a/gpperfmon/gpperfmon4.sql'
	$(INSTALL_DATA) gpperfmon41.sql '$(DESTDIR)$(libdir)a/gpperfmon/gpperfmon41.sql'
	$(INSTALL_DATA) gpperfmon42.sql '$(DESTDIR)$(libdir)a/gpperfmon/gpperfmon42.sql'
	$(INSTALL_DATA) gpperfmonC.sql '$(DESTDIR)$(libdir)a/gpperfmon/gpperfmonC.sql'

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)'
	$(MKDIR_P) '$(DESTDIR)$(libdir)/gpperfmon'

uninstall:
	rm -f '$(DESTDIR)$(bindir)/gpmmon$(X)'
	rm -f '$(DESTDIR)$(bindir)/gpsmon$(X)'
	rm -f '$(DESTDIR)$(bindir)/gpperfmon'
	rm -f '$(DESTDIR)$(bindir)/gpperfmon.py'

clean distclean maintainer-clean:
	rm -f $(GPSMON_OBJS) $(GPMMON_OBJS) gpsmon$(X) gpmmon$(X) snmptest.o snmptest$(X)

#
# Makefile for utils
#
# $PostgreSQL: pgsql/src/backend/utils/Makefile,v 1.29 2010/01/05 01:06:56 tgl Exp $
#

subdir = src/backend/utils
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

OBJS        = fmgrtab.o session_state.o
SUBDIRS     = adt cache datumstream error fmgr hash init mb misc mmgr resowner \
			   resgroup resscheduler sort time gpmon gp workfile_manager resource_manager

# location of Catalog.pm
catalogdir  = $(top_srcdir)/src/backend/catalog

include $(top_srcdir)/src/backend/common.mk

all: fmgroids.h probes.h

$(SUBDIRS:%=%-recursive): fmgroids.h

# see explanation in ../parser/Makefile
fmgroids.h: fmgrtab.c ;

pg_proc_combined.h: $(catalogdir)/Catalog.pm $(top_srcdir)/src/include/catalog/pg_proc.h $(top_srcdir)/src/include/catalog/pg_proc_gp.h
	cat $^ > $@

fmgrtab.c: Gen_fmgrtab.pl pg_proc_combined.h
	$(PERL) -I $(catalogdir) $< pg_proc_combined.h

ifneq ($(enable_dtrace), yes)
probes.h: Gen_dummy_probes.sed
endif

probes.h: probes.d
ifeq ($(enable_dtrace), yes)
	$(DTRACE) -C -h -s $< -o $@.tmp
	sed -e 's/POSTGRESQL_/TRACE_POSTGRESQL_/g' $@.tmp >$@
	rm $@.tmp
else
	sed -f $(srcdir)/Gen_dummy_probes.sed $< >$@
endif


# fmgroids.h and fmgrtab.c are in the distribution tarball, so they
# are not cleaned here.
clean:
	rm -f probes.h
	rm -f pg_proc_combied.h

maintainer-clean: clean
	rm -f fmgroids.h fmgrtab.c

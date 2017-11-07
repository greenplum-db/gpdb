#
# Makefile for utils
#
# $PostgreSQL: pgsql/src/backend/utils/Makefile,v 1.28 2008/08/01 13:16:09 alvherre Exp $
#

subdir = src/backend/utils
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

OBJS        = fmgrtab.o session_state.o
SUBDIRS     = adt cache datumstream error fmgr hash init mb misc mmgr resowner \
			   resgroup resscheduler sort time gpmon gp workfile_manager resource_manager

include $(top_srcdir)/src/backend/common.mk

all: fmgroids.h probes.h

$(SUBDIRS:%=%-recursive): fmgroids.h


# Like when creating postgres.bki, we need to inject missing columns before
# feeding pg_proc.h to Gen_fmgrtab.sh. See also src/backend/catalog/Makefile.
fmgroids.h fmgrtab.c: Gen_fmgrtab.sh $(top_srcdir)/src/include/catalog/pg_proc.h $(top_srcdir)/src/include/catalog/pg_proc_gp.h $(top_builddir)/src/include/catalog/pg_proc_gp.h $(top_srcdir)/src/backend/catalog/process_col_defaults.pl
	cat $(top_srcdir)/src/include/catalog/pg_proc.h $(top_srcdir)/src/include/catalog/pg_proc_gp.h | $(top_srcdir)/src/backend/catalog/process_col_defaults.pl > pg_proc_combined.h.tmp
	AWK='$(AWK)' $(SHELL) $< pg_proc_combined.h.tmp

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


clean:
	rm -f fmgroids.h fmgrtab.c probes.h

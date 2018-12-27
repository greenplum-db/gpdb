#
# Makefile for utils
#
# src/backend/utils/Makefile
#

subdir = src/backend/utils
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

OBJS        = fmgrtab.o session_state.o
SUBDIRS     = adt cache datumstream error fmgr gdd hash init mb misc mmgr resowner \
			   resgroup resscheduler sort time gpmon gp workfile_manager resource_manager hyperloglog

# location of Catalog.pm
catalogdir  = $(top_srcdir)/src/backend/catalog

include $(top_srcdir)/src/backend/common.mk

all: errcodes.h fmgroids.h probes.h

$(SUBDIRS:%=%-recursive): fmgroids.h

# see notes in src/backend/parser/Makefile
fmgroids.h: fmgrtab.c
	touch $@

pg_proc_combined.h: $(catalogdir)/Catalog.pm $(top_srcdir)/src/include/catalog/pg_proc.h $(top_srcdir)/src/include/catalog/pg_proc_gp.h
	cat $^ > $@

fmgrtab.c: Gen_fmgrtab.pl pg_proc_combined.h
	$(PERL) -I $(catalogdir) $< pg_proc_combined.h

errcodes.h: $(top_srcdir)/src/backend/utils/errcodes.txt generate-errcodes.pl
	$(PERL) $(srcdir)/generate-errcodes.pl $< > $@

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


# fmgroids.h, fmgrtab.c and errcodes.h are in the distribution tarball, so they
# are not cleaned here.
clean:
	rm -f probes.h
	rm -f pg_proc_combied.h

maintainer-clean: clean
	rm -f fmgroids.h fmgrtab.c errcodes.h

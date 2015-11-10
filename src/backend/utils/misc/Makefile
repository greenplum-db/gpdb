#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for utils/misc
#
# IDENTIFICATION
#    $PostgreSQL: pgsql/src/backend/utils/misc/Makefile,v 1.26 2006/07/25 03:51:21 tgl Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/utils/misc
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

SUBDIRS     := fstream


override CPPFLAGS := -I$(srcdir) $(CPPFLAGS)

OBJS = guc.o help_config.o pg_rusage.o ps_status.o superuser.o tzparser.o uriparser.o \
       faultinjector.o netcheck.o simex.o simexsys.o syncbitvector.o testutils.o atomic.o \
	   bitstream.o bitmap_compression.o size.o guc_gp.o

# This location might depend on the installation directories. Therefore
# we can't subsitute it into pg_config.h.
ifdef krb_srvtab
override CPPFLAGS += -DPG_KRB_SRVTAB='"$(krb_srvtab)"'
endif

include $(top_srcdir)/src/backend/common.mk

# guc-file is compiled as part of guc
guc.o: $(srcdir)/guc-file.c

$(srcdir)/guc-file.c: guc-file.l
ifdef FLEX
	$(FLEX) $(FLEXFLAGS) -o'$@' $<
else
	@$(missing) flex $< $@
endif

# Note: guc-file.c is not deleted by 'make clean',
# since we want to ship it in distribution tarballs.
clean: 
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir $@ || exit; done
	rm -f SUBSYS.o $(OBJS)
	@rm -f lex.yy.c

#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for utils/misc/fstream
#
#-------------------------------------------------------------------------

subdir = src/backend/utils/misc/fstream
top_builddir = ../../../../..
include $(top_builddir)/src/Makefile.global

override CPPFLAGS := -I$(srcdir) $(CPPFLAGS)

OBJS = fstream.o gfile.o

# This location might depend on the installation directories. Therefore
# we can't substitute it into pg_config.h.
ifdef krb_srvtab
override CPPFLAGS += -DPG_KRB_SRVTAB='"$(krb_srvtab)"'
endif

include $(top_srcdir)/src/backend/common.mk

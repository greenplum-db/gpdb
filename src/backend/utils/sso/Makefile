#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for utils/sso
#
# IDENTIFICATION
#    src/backend/utils/sso/Makefile
#
#-------------------------------------------------------------------------

subdir = src/backend/utils/sso
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global
override CPPFLAGS := -I$(libpq_srcdir) $(CPPFLAGS)

OBJS = ssofuncs.o

include $(top_srcdir)/src/backend/common.mk

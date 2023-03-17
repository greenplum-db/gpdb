#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for utils/resgroup
#
# IDENTIFICATION
#    src/backend/utils/resgroup/Makefile
#
#-------------------------------------------------------------------------

subdir = src/backend/utils/resgroup
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global
override CPPFLAGS := -I$(libpq_srcdir) $(CPPFLAGS)

OBJS = resgroup.o resgroup_helper.o

ifeq ($(PORTNAME),linux)
OBJS += cgroup.o
OBJS += cgroup-ops-linux-v1.o
OBJS += cgroup-ops-linux-v2.o
else
OBJS += cgroup-ops-dummy.o
endif

include $(top_srcdir)/src/backend/common.mk

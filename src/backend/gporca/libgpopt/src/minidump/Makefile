#
# Makefile for optimizer
#
# src/backend/gporca/libgpopt/src/minidump/Makefile
#

subdir = src/backend/gporca/libgpopt/src/minidump
top_builddir = ../../../../../..
include $(top_builddir)/src/Makefile.global

include $(top_srcdir)/src/backend/gporca/gporca.mk

OBJS        = CDXLMinidump.o \
              CMetadataAccessorFactory.o \
              CMiniDumperDXL.o \
              CMinidumperUtils.o \
              CSerializableMDAccessor.o \
              CSerializableOptimizerConfig.o \
              CSerializablePlan.o \
              CSerializableQuery.o \
              CSerializableStackTrace.o

include $(top_srcdir)/src/backend/common.mk


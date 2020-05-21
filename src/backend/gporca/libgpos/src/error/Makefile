#
# Makefile for optimizer
#
# src/backend/gporca/libgpos/src/error/Makefile
#

subdir = src/backend/gporca/libgpos/src/error
top_builddir = ../../../../../..
include $(top_builddir)/src/Makefile.global

include $(top_srcdir)/src/backend/gporca/gporca.mk

OBJS        = CAutoExceptionStack.o \
              CAutoTrace.o \
              CErrorContext.o \
              CErrorHandlerStandard.o \
              CException.o \
              CFSimulator.o \
              CLogger.o \
              CLoggerStream.o \
              CLoggerSyslog.o \
              CMessage.o \
              CMessageRepository.o \
              CMessageTable.o \
              CMiniDumper.o \
              CSerializable.o \
              ILogger.o

include $(top_srcdir)/src/backend/common.mk


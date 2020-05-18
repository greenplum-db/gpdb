#
# Makefile for optimizer
#
# src/backend/gporca/gporca/libgpos/src/task/Makefile
#

subdir = src/backend/gporca/libgpos/src/task
top_builddir = ../../../../../..
include $(top_builddir)/src/Makefile.global

include $(top_srcdir)/src/backend/gporca/gporca.mk

OBJS        = CAutoSuspendAbort.o \
              CAutoTaskProxy.o \
              CAutoTraceFlag.o \
              CTask.o \
              CTaskContext.o \
              CTaskLocalStorage.o \
              CTaskSchedulerFifo.o \
              CWorker.o \
              CWorkerPoolManager.o \
              ITask.o \
              IWorker.o

include $(top_srcdir)/src/backend/common.mk


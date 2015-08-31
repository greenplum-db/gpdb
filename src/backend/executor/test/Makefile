subdir=src/backend/executor
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=nodeSubplan nodeShareInputScan execAmi execWorkfile execHHashagg

# Objects from backend, which don't need to be mocked but need to be linked.
nodeSubplan_REAL_OBJS=\
	$(top_srcdir)/src/backend/executor/execUtils.o \
	$(top_srcdir)/src/backend/utils/error/elog.o \
        $(top_srcdir)/src/port/pgsleep.o \

nodeShareInputScan_REAL_OBJS=\
        $(top_srcdir)/src/port/pgsleep.o \

execAmi_REAL_OBJS=\
        $(top_srcdir)/src/port/pgsleep.o \

execWorkfile_REAL_OBJS=\
        $(top_srcdir)/src/port/pgsleep.o \

execHHashagg_REAL_OBJS=\
	$(top_srcdir)/src/backend/executor/execUtils.o \
	$(top_srcdir)/src/backend/utils/error/elog.o \
        $(top_srcdir)/src/port/pgsleep.o \

include $(top_builddir)/src/backend/mock.mk

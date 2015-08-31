subdir=src/backend/executor
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=nodeSubplan nodeShareInputScan execAmi execWorkfile execHHashagg

nodeSubplan_MOCK_OBJS=\
	$(top_srcdir)/src/backend/cdb/cdbdisp.o \
	$(top_srcdir)/src/backend/cdb/cdbexplain.o \
	$(top_srcdir)/src/backend/cdb/cdbtm.o \
	$(top_srcdir)/src/backend/cdb/motion/ic_common.o \
	$(top_srcdir)/src/backend/tcop/pquery.o

nodeShareInputScan_MOCK_OBJS=\
	$(top_srcdir)/src/backend/executor/execUtils.o \
	$(top_srcdir)/src/backend/utils/sort/tuplestorenew.o

execAmi_MOCK_OBJS=\
	$(top_srcdir)/src/backend/executor/nodeShareInputScan.o

execWorkfile_MOCK_OBJS=\
	$(top_srcdir)/src/backend/storage/file/buffile.o \
	$(top_srcdir)/src/backend/utils/workfile_manager/workfile_queryspace.o

execHHashagg_MOCK_OBJS=\
	$(top_srcdir)/src/backend/utils/workfile_manager/workfile_file.o

include $(top_builddir)/src/backend/mock.mk

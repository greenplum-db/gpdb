subdir=src/backend/fts
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=ftsprobe

include $(top_builddir)/src/backend/mock.mk

ftsprobe.t: \
	$(MOCK_DIR)/backend/utils/error/elog_mock.o \
	$(MOCK_DIR)/backend/cdb/cdbthreadlog_mock.o

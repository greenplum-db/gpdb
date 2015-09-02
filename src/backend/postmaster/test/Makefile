subdir=src/backend/postmaster
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=backoff syslogger

include $(top_builddir)/src/backend/mock.mk

backoff.t: $(MOCK_DIR)/backend/utils/error/assert_mock.o

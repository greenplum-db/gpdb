subdir=src/backend/access/external
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=url_curl

include $(top_srcdir)/src/backend/mock.mk

url_curl.t: $(MOCK_DIR)/backend/cdb/cdbutil_mock.o

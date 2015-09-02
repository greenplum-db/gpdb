subdir=src/backend/access/common
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=reloptions tupdesc

include $(top_builddir)/src/backend/mock.mk

tupdesc.t: $(MOCK_DIR)/backend/utils/resowner/resowner_mock.o

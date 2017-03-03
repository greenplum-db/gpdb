subdir=src/backend/utils/adt
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=varlena

include $(top_builddir)/src/backend/mock.mk

varlena.t: \
	$(MOCK_DIR)/backend/utils/error/assert_mock.o \
	$(MOCK_DIR)/backend/utils/mb/mbutils_mock.o \
	$(MOCK_DIR)/backend/utils/mb/wchar_mock.o

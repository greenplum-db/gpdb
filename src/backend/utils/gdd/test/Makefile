subdir=src/backend/utils/gdd
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=gdd

include $(top_srcdir)/src/backend/mock.mk

override CPPFLAGS+= -I ..

gdd.t: \
	$(MOCK_DIR)/backend/access/hash/hash_mock.o \
	$(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o 

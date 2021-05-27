subdir=src/backend/utils/gdd
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=gdddetector

include $(top_srcdir)/src/backend/mock.mk

gdddetector.t: \
	$(MOCK_DIR)/backend/access/hash/hash_mock.o \
	$(MOCK_DIR)/backend/utils/fmgr/fmgr_mock.o 

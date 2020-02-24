subdir=src/backend/postmaster
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=checkpointer

include $(top_builddir)/src/backend/mock.mk
checkpointer.t: $(MOCK_DIR)/backend/storage/lmgr/lwlock_mock.o

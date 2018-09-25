subdir=src/backend/postmaster
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=syslogger checkpointer autovacuum

include $(top_builddir)/src/backend/mock.mk
checkpointer.t: $(MOCK_DIR)/backend/storage/lmgr/lwlock_mock.o

autovacuum_test.o: FORCE

.PHONY: FORCE

test_autovacuum: autovacuum.t
	./autovacuum.t

FORCE:

subdir=src/backend/utils/time
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=sharedsnapshot

include $(top_builddir)/src/backend/mock.mk

sharedsnapshot.t: \
	$(MOCK_DIR)/backend/storage/ipc/shmem_mock.o \
	$(MOCK_DIR)/backend/storage/lmgr/lwlock_mock.o \
	$(MOCK_DIR)/backend/utils/misc/faultinjector_mock.o

.PHONY:
%-check: %.t
	$(MKDIR_P) 'base/pgsql_tmp'
	./$*.t

.PHONY:
%-clean:
	rm -rf base
	rm -f $*.t $*_test.o

subdir=src/backend/utils/misc
top_builddir=../../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=guc ps_status bitstream bitmap_compression

guc_MOCK_OBJS=\
	$(top_srcdir)/src/backend/storage/ipc/shmem.o \
	$(top_srcdir)/src/backend/utils/misc/superuser.o

include $(top_builddir)/src/backend/mock.mk

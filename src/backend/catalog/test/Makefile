top_builddir = ../../../..
subdir = src/backend/catalog


include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/src/Makefile.mock


TARGETS += storage_tablespace


storage_tablespace.t: $(top_srcdir)/src/backend/catalog/storage_tablespace.o
	make -C $(top_srcdir)/src/backend/catalog/ && \
	$(CC) $(CFLAGS) $(LDFLAGS) $(CMOCKERY_OBJS) $(CPPFLAGS) \
		$(top_srcdir)/src/backend/catalog/storage_tablespace.o \
		storage_tablespace_test.c \
		-o storage_tablespace_test.o && ./storage_tablespace_test.o


check: storage_tablespace.t
subdir=contrib/pg_upgrade/test
top_builddir=../../..
top_srcdir=../../..
pg_upgrade_directory=../


TARGETS = cluster_test


include $(top_builddir)/src/Makefile.mock
include $(top_builddir)/src/Makefile.global


test_flags = -I$(pg_upgrade_directory) -I$(libpq_srcdir)


code_under_test = $(pg_upgrade_directory)/cluster.o


cluster_test.t: $(CMOCKERY_OBJS) cluster_test.c
	make -C ../ install

	$(CC) $(CFLAGS) $(CPPFLAGS) $(test_flags) \
		$(code_under_test) \
		$(pg_upgrade_dependencies) \
		$^ -o $@

subdir=contrib/pg_upgrade/test
top_builddir=../../../..
top_srcdir=../../../..
pg_upgrade_directory=../..


TARGETS = cluster_test


include $(top_builddir)/src/Makefile.mock
include $(top_builddir)/src/Makefile.global


test_flags = -I$(pg_upgrade_directory) -I$(libpq_srcdir)


code_under_test = $(pg_upgrade_directory)/cluster.o


.PHONY: make_dependencies FORCE
make_dependencies: FORCE
	echo "installing make dependencies"
	make -C $(pg_upgrade_directory) all
FORCE:


cluster_test.t: cluster_test.c $(CMOCKERY_OBJS) make_dependencies
	$(CC) $(CFLAGS) $(CPPFLAGS) $(test_flags) \
		$(code_under_test) \
		$(CMOCKERY_OBJS) \
		$(pg_upgrade_dependencies) \
		$< \
		-o $@

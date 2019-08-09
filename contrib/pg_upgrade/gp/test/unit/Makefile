top_srcdir=../../../../..
top_builddir=../../../../..
pg_upgrade_directory=$(top_srcdir)/contrib/pg_upgrade
subdir=$(pg_upgrade_directory)/gp/test/unit


TARGETS = check_greenplum_test


include $(top_srcdir)/src/Makefile.mock
include $(top_srcdir)/src/Makefile.global


test_flags = -I $(pg_upgrade_directory) -I $(libpq_srcdir)


check_greenplum_test.t: $(CMOCKERY_OBJS) check_greenplum_test.c
	make -C $(pg_upgrade_directory) all

	$(CC) $(CFLAGS) $(CPPFLAGS) $(test_flags) \
		$(pg_upgrade_directory)/gp/check_greenplum_internal.o \
		$^ \
		-o check_greenplum_test.t

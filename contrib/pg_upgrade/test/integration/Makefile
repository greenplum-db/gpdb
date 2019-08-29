subdir=contrib/pg_upgrade/test/integration
top_srcdir=../../../..
top_builddir=../../../..
pg_upgrade_directory=../..


TARGETS = greenplum_five_to_greenplum_six_upgrade_test


include $(top_srcdir)/src/Makefile.mock
include $(top_srcdir)/src/Makefile.global


test_flags = -I $(pg_upgrade_directory) \
	-I$(libpq_srcdir) \
	-g -O0


compile_test = $(CC) $(CFLAGS) $(CPPFLAGS) $(test_flags) 


integration_test_dependencies = $(CMOCKERY_OBJS) $(libpq_pgport) 


greenplum_five_to_greenplum_six_upgrade_test.t: greenplum_five_to_greenplum_six_upgrade_test.c $(CMOCKERY_OBJS)
	$(compile_test) \
		$(integration_test_dependencies) \
		$< \
		-o $@
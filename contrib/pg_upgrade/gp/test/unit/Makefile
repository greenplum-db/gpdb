top_srcdir=../../../../..
top_builddir=../../../../..
pg_upgrade_directory=$(top_srcdir)/contrib/pg_upgrade
subdir=$(pg_upgrade_directory)/gp/test/unit


TARGETS = check_greenplum_test \
	user_defined_index_check_test


include $(top_srcdir)/src/Makefile.mock
include $(top_srcdir)/src/Makefile.global


test_flags = -I $(pg_upgrade_directory) -I $(libpq_srcdir) \
	-fsanitize=undefined \
	-fsanitize=address


compile_test = $(CC) $(CFLAGS) $(CPPFLAGS) $(test_flags)


.PHONY: make_dependencies FORCE

make_dependencies: FORCE
	# compile using pg_upgrade Makefile
	make -C $(pg_upgrade_directory) all
FORCE:


unit_test_dependencies = $(CMOCKERY_OBJS)


check_greenplum_test.t: check_greenplum_test.c make_dependencies $(unit_test_dependencies)
	$(compile_test) \
		$(pg_upgrade_directory)/gp/check_greenplum_internal.o \
		$(unit_test_dependencies) \
		$< \
		-o $@

user_defined_index_check_test.t: user_defined_index_check_test.c make_dependencies $(unit_test_dependencies) 
	$(compile_test) \
		$(pg_upgrade_directory)/gp/user_defined_indexes_check.o \
		$(unit_test_dependencies) \
		$< \
		-o $@

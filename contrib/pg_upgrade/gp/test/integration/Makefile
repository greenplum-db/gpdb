subdir=contrib/pg_upgrade/gp/test/integration
top_srcdir=../../../../..
top_builddir=../../../../..
pg_upgrade_directory=../../..
gp_checks_directory=../..


TARGETS = check_covering_aoindex_test \
	user_defined_indexes_check_test \
	cluster_test


PG_LIBS = $(libpq_pgport)


include $(top_srcdir)/src/Makefile.mock
include $(top_srcdir)/src/Makefile.global


test_flags = -I $(pg_upgrade_directory) \
	-I$(libpq_srcdir) -g -O0


code_under_test = $(gp_checks_directory)/checks.o \
	$(gp_checks_directory)/user_defined_indexes_check.o \
	$(gp_checks_directory)/cluster.o


pg_upgrade_dependencies = $(pg_upgrade_directory)/server.o \
	$(pg_upgrade_directory)/util.o \
	$(pg_upgrade_directory)/relfilenode.o \
	$(pg_upgrade_directory)/info.o \
	$(pg_upgrade_directory)/aomd_filehandler.o \
	$(pg_upgrade_directory)/gpdb4_heap_convert.o \
	$(pg_upgrade_directory)/file.o \
	$(pg_upgrade_directory)/version_gp.o \
	$(pg_upgrade_directory)/parallel.o \
	$(pg_upgrade_directory)/file_gp.o \
	$(pg_upgrade_directory)/reporting.o


integration_test_dependencies = $(CMOCKERY_OBJS) test_utils.c pg_upgrade_fakes.c


.PHONY: make_dependencies FORCE


make_dependencies: FORCE
	# compile using pg_upgrade Makefile
	make -C $(pg_upgrade_directory) all
FORCE:


compile_test = $(CC) $(CFLAGS) $(CPPFLAGS) $(test_flags) 


check_covering_aoindex_test.t: check_covering_aoindex_test.c make_dependencies $(integration_test_dependencies) 
	$(compile_test) \
		$(code_under_test) \
		$(pg_upgrade_dependencies) \
		$(integration_test_dependencies) \
		$< \
		$(PG_LIBS) \
		-o $@


user_defined_indexes_check_test.t: user_defined_indexes_check_test.c make_dependencies $(integration_test_dependencies)
	$(compile_test) \
		$(code_under_test) \
		$(pg_upgrade_dependencies) \
		$(integration_test_dependencies) \
		$< \
		$(PG_LIBS) \
		-o $@


cluster_test.t: cluster_test.c make_dependencies $(integration_test_dependencies)
	$(compile_test) \
		$(code_under_test) \
		$(pg_upgrade_dependencies) \
		$(integration_test_dependencies) \
		$< \
		$(PG_LIBS) \
		-o $@

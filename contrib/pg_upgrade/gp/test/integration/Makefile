subdir=contrib/pg_upgrade/gp/test/integration
top_srcdir=../../../../..
top_builddir=../../../../..
pg_upgrade_directory=../../..
gp_checks_directory=../..


TARGETS = check_covering_aoindex_test check_non_default_extensions_test
PG_LIBS = $(libpq_pgport)


include $(top_srcdir)/src/Makefile.mock
include $(top_srcdir)/src/Makefile.global


test_flags = -I $(pg_upgrade_directory) \
	-I$(libpq_srcdir) -g -O0


code_under_test = $(gp_checks_directory)/checks.o \
 	$(gp_checks_directory)/string_utils.o


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

check_covering_aoindex_test.t: make_dependencies $(integration_test_dependencies) check_covering_aoindex_test.c 
	$(CC) $(CFLAGS) $(CPPFLAGS) $(test_flags) \
		$(code_under_test) \
		$(pg_upgrade_dependencies) \
		$(integration_test_dependencies) \
		./check_covering_aoindex_test.c \
		$(PG_LIBS) \
		-o $@

check_non_default_extensions_test.t: make_dependencies $(integration_test_dependencies) check_non_default_extensions_test.c 
	$(CC) $(CFLAGS) $(CPPFLAGS) $(test_flags) \
		$(code_under_test) \
		$(pg_upgrade_dependencies) \
		$(integration_test_dependencies) \
		./check_non_default_extensions_test.c \
		$(PG_LIBS) \
		-o $@

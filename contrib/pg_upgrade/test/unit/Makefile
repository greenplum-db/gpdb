subdir=contrib/pg_upgrade/test/unit
top_srcdir=../../../..
top_builddir=../../../..
pg_upgrade_directory=../..
pg_upgrade_greenplum_directory=../../greenplum

OBJS = tablespace_test.o \
	old_tablespace_file_contents_test.o \
	old_tablespace_file_parser_test.o \
	check_gp_test.o \
	mock_server.o \
	mock_util.o \
	info_gp_test.o

TARGETS = tablespace_test \
	old_tablespace_file_contents_test \
	old_tablespace_file_parser_test \
	tablespace_gp_test \
	check_gp_test \
	info_gp_test

include $(top_srcdir)/src/Makefile.global
include $(top_srcdir)/src/Makefile.mock

override CPPFLAGS += -I$(pg_upgrade_directory) \
	-I$(libpq_srcdir)

test_dependencies = $(CMOCKERY_OBJS)

debugging_flags = -Og -g -fsanitize=address -Wextra -pedantic-errors -Werror

compile_test = $(CC) $(CFLAGS) $(debugging_flags) $^ $(libpq_pgport) $(LDFLAGS) -o $@ 

tablespace_test.t: tablespace_test.o $(pg_upgrade_directory)/tablespace.o $(test_dependencies)
	$(compile_test)

old_tablespace_file_contents_test.t: old_tablespace_file_contents_test.o $(pg_upgrade_greenplum_directory)/old_tablespace_file_contents.o $(test_dependencies)
	$(compile_test)

old_tablespace_file_parser_test.t: old_tablespace_file_parser_test.o $(pg_upgrade_greenplum_directory)/old_tablespace_file_parser.o $(test_dependencies)
	$(compile_test)

tablespace_gp_test.t: tablespace_gp_test.o $(pg_upgrade_greenplum_directory)/tablespace_gp.o $(test_dependencies)
	$(compile_test)

info_gp_test.t: info_gp_test.o $(pg_upgrade_greenplum_directory)/info_gp.o $(test_dependencies)
	$(compile_test)

check_gp_test.t: mock_util.o \
	mock_server.o \
	check_gp_test.o \
	$(MOCK_DIR)/backend/libpq/fe-exec_mock.o \
	$(test_dependencies) \
	$(pg_upgrade_greenplum_directory)/check_gp.o \
	$(pg_upgrade_greenplum_directory)/option_gp.o
	$(compile_test)

clean:
	rm -f *.t *.o
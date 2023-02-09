#
# Makefile for isolation2 tests
#

subdir = src/test/isolation2
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

NAME = isolation2_regress
OBJS = isolation2_regress.o workfile_mgr_test.o
include $(top_srcdir)/src/Makefile.shlib

ifeq ($(PORTNAME), win32)
LDLIBS += -lws2_32
endif

override CPPFLAGS := -I$(srcdir) -I$(libpq_srcdir) -I$(srcdir)/../regress $(CPPFLAGS)
override LDLIBS := $(libpq_pgport) $(LDLIBS)

all: pg_isolation2_regress$(X) all-lib data extended_protocol_test test_parallel_retrieve_cursor_extended_query test_parallel_retrieve_cursor_extended_query_error

extended_protocol_test: extended_protocol_test.c
	$(CC) $(CPPFLAGS) -I$(top_builddir)/src/interfaces/libpq -L$(GPHOME)/lib -L$(top_builddir)/src/interfaces/libpq  -o $@ $< -lpq

test_parallel_retrieve_cursor_extended_query: test_parallel_retrieve_cursor_extended_query.c
	$(CC) $(CPPFLAGS) -I$(top_builddir)/src/interfaces/libpq -L$(GPHOME)/lib -L$(top_builddir)/src/interfaces/libpq  -o $@ $< -lpq

test_parallel_retrieve_cursor_extended_query_error: test_parallel_retrieve_cursor_extended_query_error.c
	$(CC) $(CPPFLAGS) -I$(top_builddir)/src/interfaces/libpq -L$(GPHOME)/lib -L$(top_builddir)/src/interfaces/libpq  -o $@ $< -lpq

pg_regress.o:
	$(MAKE) -C $(top_builddir)/src/test/regress pg_regress.o
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/regress/pg_regress.o .

gpstringsubs.pl:
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/regress/gpstringsubs.pl

gpdiff.pl: atmsort.pm explain.pm GPTest.pm
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/regress/gpdiff.pl

GPTest.pm:
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/regress/GPTest.pm

atmsort.pm:
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/regress/atmsort.pm

explain.pm:
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/regress/explain.pm

data:
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/regress/data

pg_isolation2_regress$(X): isolation2_main.o pg_regress.o submake-libpq submake-libpgport
	$(CC) $(CFLAGS) $(filter %.o,$^) $(libpq_pgport) $(LDFLAGS) $(LDFLAGS_EX) $(LIBS) -o $@

clean distclean:
	rm -f pg_isolation2_regress$(X) $(OBJS) isolation2_main.o isolation2_regress.so
	rm -f pg_regress.o test_parallel_retrieve_cursor_extended_query test_parallel_retrieve_cursor_extended_query_error
	rm -f gpstringsubs.pl gpdiff.pl atmsort.pm explain.pm
	rm -f data
	rm -rf $(pg_regress_clean_files)

install: all gpdiff.pl gpstringsubs.pl

installcheck: install installcheck-parallel-retrieve-cursor installcheck-ic-tcp installcheck-ic-proxy
	$(pg_isolation2_regress_installcheck) --init-file=$(top_builddir)/src/test/regress/init_file --init-file=./init_file_isolation2 --schedule=$(srcdir)/isolation2_schedule

installcheck-resgroup: install
	$(pg_isolation2_regress_installcheck) --init-file=$(top_builddir)/src/test/regress/init_file --init-file=./init_file_resgroup --dbname=isolation2resgrouptest --schedule=$(srcdir)/isolation2_resgroup_v1_schedule

installcheck-resgroup-v2: install
	$(pg_isolation2_regress_installcheck) --init-file=$(top_builddir)/src/test/regress/init_file --init-file=./init_file_resgroup --dbname=isolation2resgrouptest --schedule=$(srcdir)/isolation2_resgroup_v2_schedule

installcheck-parallel-retrieve-cursor: install
	$(pg_isolation2_regress_installcheck) $(EXTRA_REGRESS_OPTS) --init-file=$(top_builddir)/src/test/regress/init_file --init-file=./init_file_parallel_retrieve_cursor --bindir='$(bindir)' --inputdir=$(srcdir) --dbname=isolation2parallelretrcursor --schedule=$(srcdir)/parallel_retrieve_cursor_schedule

installcheck-mirrorless: install
	$(pg_isolation2_regress_installcheck) $(EXTRA_REGRESS_OPTS) --init-file=$(top_builddir)/src/test/regress/init_file --init-file=./init_file_isolation2 --schedule=$(srcdir)/mirrorless_schedule --dbname=isolation2-mirrorless

installcheck-ic-tcp: install
ifeq ($(findstring gp_interconnect_type=tcp,$(PGOPTIONS)),gp_interconnect_type=tcp)
	$(pg_isolation2_regress_installcheck) $(EXTRA_REGRESS_OPTS) --init-file=$(top_builddir)/src/test/regress/init_file --init-file=./init_file_isolation2 --bindir='$(bindir)' --inputdir=$(srcdir) --schedule=$(srcdir)/isolation2_ic_tcp_schedule
endif

installcheck-ic-proxy: install
ifeq ($(findstring gp_interconnect_type=proxy,$(PGOPTIONS)),gp_interconnect_type=proxy)
	$(pg_isolation2_regress_installcheck) $(EXTRA_REGRESS_OPTS) --init-file=$(top_builddir)/src/test/regress/init_file --init-file=./init_file_isolation2 --bindir='$(bindir)' --inputdir=$(srcdir) --schedule=$(srcdir)/isolation2_ic_proxy_schedule
endif

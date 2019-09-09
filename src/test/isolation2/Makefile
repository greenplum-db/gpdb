#
# Makefile for isolation2 tests
#

subdir = src/test/isolation2
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

NAME = isolation2_regress
OBJS = isolation2_regress.o
include $(top_srcdir)/src/Makefile.shlib

# where to find psql for testing an existing installation
PSQLDIR = $(bindir)

ifeq ($(PORTNAME), win32)
LDLIBS += -lws2_32
endif

override CPPFLAGS := -I$(srcdir) -I$(libpq_srcdir) -I$(srcdir)/../regress $(CPPFLAGS)
override LDLIBS := $(libpq_pgport) $(LDLIBS)

all: test_python pg_isolation2_regress$(X) all-lib data scan_flaky_fault_injectors

test_python:
	python helpers_test.py

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

.PHONY: scan_flaky_fault_injectors
scan_flaky_fault_injectors:
	$(top_builddir)/src/test/regress/scan_flaky_fault_injectors.sh

pg_isolation2_regress$(X): isolation2_main.o pg_regress.o submake-libpq submake-libpgport
	$(CC) $(CFLAGS) $(filter %.o,$^) $(libpq_pgport) $(LDFLAGS) $(LDFLAGS_EX) $(LIBS) -o $@

clean distclean:
	rm -f pg_isolation2_regress$(X) $(OBJS) isolation2_main.o
	rm -f pg_regress.o
	rm -f gpstringsubs.pl gpdiff.pl atmsort.pm explain.pm
	rm -f data
	rm -rf $(pg_regress_clean_files)

install: all gpdiff.pl gpstringsubs.pl

installcheck: install
	./pg_isolation2_regress --init-file=$(top_builddir)/src/test/regress/init_file --init-file=./init_file_isolation2 --psqldir='$(PSQLDIR)' --inputdir=$(srcdir) --schedule=$(srcdir)/isolation2_schedule

installcheck-resgroup: install
	./pg_isolation2_regress --init-file=$(top_builddir)/src/test/regress/init_file --init-file=./init_file_resgroup --psqldir='$(PSQLDIR)' --inputdir=$(srcdir) --dbname=isolation2resgrouptest --schedule=$(srcdir)/isolation2_resgroup_schedule

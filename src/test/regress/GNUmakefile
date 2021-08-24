#-------------------------------------------------------------------------
#
# GNUmakefile--
#    Makefile for src/test/regress (the regression tests)
#
# Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
# Portions Copyright (c) 1994, Regents of the University of California
#
# src/test/regress/GNUmakefile
#
#-------------------------------------------------------------------------

subdir = src/test/regress
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

# file with extra config for temp build
TEMP_CONF =
ifdef TEMP_CONFIG
TEMP_CONF += --temp-config=$(TEMP_CONFIG)
endif

# where to find psql for testing an existing installation
PSQLDIR = $(bindir)

# maximum simultaneous connections for parallel tests
MAXCONNOPT =
ifdef MAX_CONNECTIONS
MAXCONNOPT += --max-connections=$(MAX_CONNECTIONS)
endif

UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
	EXTRA_TESTS += --schedule=$(srcdir)/linux_schedule
endif
# stuff to pass into build of pg_regress
EXTRADEFS = '-DHOST_TUPLE="$(host_tuple)"' \
	'-DMAKEPROG="$(MAKE)"' \
	'-DSHELLPROG="$(SHELL)"' \
	'-DDLSUFFIX="$(DLSUFFIX)"'


# The frontend doesn't need everything that's in LIBS, some are backend only
LIBS := $(filter-out -lresolv -lbz2, $(LIBS))
# This program isn't interactive, so doesn't need these
LIBS := $(filter-out -lreadline -ledit -ltermcap -lncurses -lcurses -lcurl -lssl -lz, $(LIBS))

##
## Prepare for tests
##

# Build regression test driver

all: pg_regress$(X) scan_flaky_fault_injectors file_monitor

pg_regress$(X): pg_regress.o pg_regress_main.o | submake-libpgport
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LDFLAGS_EX) $(LIBS) -o $@

# dependencies ensure that path changes propagate
pg_regress.o: pg_regress.c $(top_builddir)/src/port/pg_config_paths.h
pg_regress.o: override CPPFLAGS += -I$(top_builddir)/src/port $(EXTRADEFS)
regress_gp.o: override CFLAGS += -I$(top_builddir)/src/interfaces/libpq
regress_gp.o: override LDFLAGS += -L$(top_builddir)/src/interfaces/libpq

.PHONY: scan_flaky_fault_injectors
scan_flaky_fault_injectors:
	$(top_builddir)/src/test/regress/scan_flaky_fault_injectors.sh

file_monitor:
	$(CC) -o /tmp/file_monitor $(top_builddir)/src/test/regress/file_monitor.c

twophase_pqexecparams: twophase_pqexecparams.c
	$(CC) $(CPPFLAGS) -I$(top_builddir)/src/interfaces/libpq -L$(GPHOME)/lib -L$(top_builddir)/src/interfaces/libpq  -o $@ $< -lpq

$(top_builddir)/src/port/pg_config_paths.h: $(top_builddir)/src/Makefile.global
	$(MAKE) -C $(top_builddir)/src/port pg_config_paths.h

install: all installdirs
	$(INSTALL_PROGRAM) pg_regress$(X) '$(DESTDIR)$(pgxsdir)/$(subdir)/pg_regress$(X)'
	$(INSTALL_PROGRAM) gpdiff.pl '$(DESTDIR)$(pgxsdir)/$(subdir)/gpdiff.pl'
	$(INSTALL_PROGRAM) gpstringsubs.pl '$(DESTDIR)$(pgxsdir)/$(subdir)/gpstringsubs.pl'
	$(INSTALL_PROGRAM) atmsort.pl '$(DESTDIR)$(pgxsdir)/$(subdir)/atmsort.pl'
	$(INSTALL_PROGRAM) atmsort.pm '$(DESTDIR)$(pgxsdir)/$(subdir)/atmsort.pm'
	$(INSTALL_PROGRAM) explain.pl '$(DESTDIR)$(pgxsdir)/$(subdir)/explain.pl'
	$(INSTALL_PROGRAM) explain.pm '$(DESTDIR)$(pgxsdir)/$(subdir)/explain.pm'
	$(INSTALL_PROGRAM) GPTest.pm '$(DESTDIR)$(pgxsdir)/$(subdir)/GPTest.pm'

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(pgxsdir)/$(subdir)'

uninstall:
	rm -f '$(DESTDIR)$(pgxsdir)/$(subdir)/pg_regress$(X)'
	rm -f '$(DESTDIR)$(pgxsdir)/$(subdir)/gpdiff.pl'
	rm -f '$(DESTDIR)$(pgxsdir)/$(subdir)/gpstringsubs.pl'
	rm -f '$(DESTDIR)$(pgxsdir)/$(subdir)/atmsort.pl'
	rm -f '$(DESTDIR)$(pgxsdir)/$(subdir)/atmsort.pm'
	rm -f '$(DESTDIR)$(pgxsdir)/$(subdir)/explain.pl'
	rm -f '$(DESTDIR)$(pgxsdir)/$(subdir)/explain.pm'
	rm -f '$(DESTDIR)$(pgxsdir)/$(subdir)/GPTest.pm'


# Build dynamically-loaded object file for CREATE FUNCTION ... LANGUAGE C.

NAME = regress
OBJS = regress.o

OBJS += regress_gp.o partindex.o

include $(top_srcdir)/src/Makefile.shlib

all: all-lib

# Test input and expected files.  These are created by pg_regress itself, so we
# don't have a rule to create them.  We do need rules to clean them however.
input_files = $(patsubst $(srcdir)/input/%.source,sql/%.sql, $(wildcard $(srcdir)/input/*.source))
output_files := $(patsubst $(srcdir)/output/%.source,expected/%.out, $(wildcard $(srcdir)/output/*.source))

# not installed by default

regress_data_files = \
	$(filter-out $(addprefix $(srcdir)/,$(output_files)),$(wildcard $(srcdir)/expected/*.out)) \
	$(wildcard $(srcdir)/input/*.source) \
	$(wildcard $(srcdir)/output/*.source) \
	$(wildcard $(srcdir)/output/uao_dml/*.source) \
	$(wildcard $(srcdir)/output/uao_ddl/*.source) \
	$(filter-out $(addprefix $(srcdir)/,$(input_files)),$(wildcard $(srcdir)/sql/*.sql)) \
	$(wildcard $(srcdir)/data/*.data) \
	$(srcdir)/parallel_schedule $(srcdir)/serial_schedule $(srcdir)/resultmap

install-tests: all install install-lib installdirs-tests
	$(MAKE) -C $(top_builddir)/contrib/spi install
	for file in $(regress_data_files); do \
	  $(INSTALL_DATA) $$file '$(DESTDIR)$(pkglibdir)/regress/'$$file || exit; \
	done

installdirs-tests: installdirs
	$(MKDIR_P)  $(patsubst $(srcdir)/%/,'$(DESTDIR)$(pkglibdir)/regress/%',$(sort $(dir $(regress_data_files))))


# Get some extra C modules from contrib/spi and contrib/dummy_seclabel...

all: refint$(DLSUFFIX) autoinc$(DLSUFFIX) dummy_seclabel$(DLSUFFIX) tablespace-setup hooktest query_info_hook_test

refint$(DLSUFFIX): $(top_builddir)/contrib/spi/refint$(DLSUFFIX)
	cp $< $@

autoinc$(DLSUFFIX): $(top_builddir)/contrib/spi/autoinc$(DLSUFFIX)
	cp $< $@

dummy_seclabel$(DLSUFFIX): $(top_builddir)/contrib/dummy_seclabel/dummy_seclabel$(DLSUFFIX)
	cp $< $@

$(top_builddir)/contrib/spi/refint$(DLSUFFIX): | submake-contrib-spi ;

$(top_builddir)/contrib/spi/autoinc$(DLSUFFIX): | submake-contrib-spi ;

$(top_builddir)/contrib/dummy_seclabel/dummy_seclabel$(DLSUFFIX): | submake-contrib-dummy_seclabel ;

submake-contrib-spi:
	$(MAKE) -C $(top_builddir)/contrib/spi

submake-contrib-dummy_seclabel:
	$(MAKE) -C $(top_builddir)/contrib/dummy_seclabel

.PHONY: submake-contrib-spi submake-contrib-dummy_seclabel

# Tablespace setup

.PHONY: tablespace-setup
tablespace-setup:
	rm -rf ./testtablespace ./testtablespace_*
	mkdir -p ./testtablespace ./testtablespace_otherloc ./testtablespace_unlogged \
	./testtablespace_existing_version_dir/1/GPDB_99_399999991/ \
	./testtablespace_existing_version_dir/2/GPDB_99_399999991/ \
	./testtablespace_existing_version_dir/3/GPDB_99_399999991/ \
	./testtablespace_existing_version_dir/4/GPDB_99_399999991/ \
	./testtablespace_existing_version_dir/5/GPDB_99_399999991/ \
	./testtablespace_existing_version_dir/6/GPDB_99_399999991/ \
	./testtablespace_existing_version_dir/7/GPDB_99_399999991/ \
	./testtablespace_existing_version_dir/8/GPDB_99_399999991/ \
        ./testtablespace_1111111111222222222233333333334444444444555555555566666666667777777777888888888899999999990000000000/ \
	./testtablespace_default_tablespace \
	./testtablespace_temp_tablespace \
	./testtablespace_mytempsp0 \
	./testtablespace_mytempsp1 \
	./testtablespace_mytempsp2 \
	./testtablespace_mytempsp3 \
	./testtablespace_mytempsp4 \
	./testtablespace_database_tablespace

.PHONY: hooktest
hooktest:
	$(MAKE) -C $< $@

.PHONY: query_info_hook_test
query_info_hook_test:
	$(MAKE) -C $< $@

##
## Run tests
##

REGRESS_OPTS = --dlpath=. --init-file=$(srcdir)/init_file $(EXTRA_REGRESS_OPTS)

check: all tablespace-setup
	$(pg_regress_check) $(REGRESS_OPTS) --schedule=$(srcdir)/parallel_schedule $(MAXCONNOPT) $(TEMP_CONF) $(EXTRA_TESTS)

check-tests: all tablespace-setup
	$(pg_regress_check) $(REGRESS_OPTS) $(MAXCONNOPT) $(TEMP_CONF) $(TESTS) $(EXTRA_TESTS)

#installcheck: all tablespace-setup
#	$(pg_regress_installcheck) $(REGRESS_OPTS) --schedule=$(srcdir)/serial_schedule $(EXTRA_TESTS)
installcheck: installcheck-good

installcheck-small: all
	$(pg_regress_installcheck) $(REGRESS_OPTS) --schedule=$(srcdir)/parallel_schedule $(EXTRA_TESTS)

installcheck-good: all twophase_pqexecparams hooktest query_info_hook_test
	$(pg_regress_installcheck) $(REGRESS_OPTS) --schedule=$(srcdir)/parallel_schedule --schedule=$(srcdir)/greenplum_schedule $(EXTRA_TESTS)

installcheck-parallel: all tablespace-setup
	$(pg_regress_installcheck) $(REGRESS_OPTS) --schedule=$(srcdir)/parallel_schedule $(MAXCONNOPT) $(EXTRA_TESTS)

installcheck-tests: all tablespace-setup
	$(pg_regress_installcheck) $(REGRESS_OPTS) $(TESTS) $(EXTRA_TESTS)

ifeq ($(BUILD_TYPE),prod)
.PHONY: installcheck-icudp
installcheck-icudp:
	+@echo "icudp tests can not run in production builds, skipped"
else
installcheck-icudp: all tablespace-setup
	$(pg_regress_installcheck) $(REGRESS_OPTS) --schedule=$(srcdir)/icudp_schedule --dbname=regression_icudp $(EXTRA_TESTS)
endif

testbouncer: all
	bash -c "bouncer/setup.sh"
	$(pg_regress_check)  --port=65432 --schedule=./minimal_schedule --host=`hostname` $(EXTRA_TESTS)
	bash -c "bouncer/stop.sh"

standbycheck: all
	$(pg_regress_installcheck) $(REGRESS_OPTS) --schedule=$(srcdir)/standby_schedule  $(EXTRA_TESTS)

# old interfaces follow...

runcheck: check
runtest: installcheck
runtest-parallel: installcheck-parallel

bigtest: all tablespace-setup
	$(pg_regress_installcheck) $(REGRESS_OPTS) --schedule=$(srcdir)/serial_schedule numeric_big

bigcheck: all tablespace-setup
	$(pg_regress_check) $(REGRESS_OPTS) --schedule=$(srcdir)/parallel_schedule $(MAXCONNOPT) numeric_big

##
## Clean up
##

clean distclean maintainer-clean: clean-lib
# things built by `all' target
	$(MAKE) -C $(top_builddir)/contrib/spi clean
	$(MAKE) -C hooktest/ clean
	$(MAKE) -C query_info_hook_test/ clean
	rm -f twophase_pqexecparams
# things created by dynamic configs
	find sql -type l | xargs rm -f
	find expected -type l | xargs rm -f
	rm -f $(OBJS) refint$(DLSUFFIX) autoinc$(DLSUFFIX) dummy_seclabel$(DLSUFFIX)
	rm -f pg_regress_main.o pg_regress.o pg_regress$(X)
# things created by various check targets
	rm -f $(output_files) $(input_files)
	rm -rf ./testtablespace ./testtablespace_*
	rm -rf $(pg_regress_clean_files)
	rm -f data/pg_class32.data
	rm -f gmon.out
	rm -f $(srcdir)/sql/cppudf.sql

#-------------------------------------------------------------------------
#
# GNUmakefile--
#    Makefile for src/test/regress (the regression tests)
#
# Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
# Portions Copyright (c) 1994, Regents of the University of California
#
# $PostgreSQL: pgsql/src/test/regress/GNUmakefile,v 1.77 2009/01/01 17:24:04 momjian Exp $
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

# Greenplum regression tests use $PGPORT
PGPORT?=15432

# where to find psql for testing an existing installation
PSQLDIR = $(bindir)

# default encoding
##MULTIBYTE = LATIN1

# maximum simultaneous connections for parallel tests
MAXCONNOPT =
ifdef MAX_CONNECTIONS
MAXCONNOPT += --max-connections=$(MAX_CONNECTIONS)
endif

# locale
NOLOCALE =
ifdef NO_LOCALE
NOLOCALE += --no-locale
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

all: submake-libpgport pg_regress$(X)

pg_regress$(X): pg_regress.o pg_regress_main.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LDFLAGS_EX) $(LIBS) -o $@

# dependencies ensure that path changes propagate
pg_regress.o: pg_regress.c $(top_builddir)/src/port/pg_config_paths.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -I$(top_builddir)/src/port $(EXTRADEFS) -c -o $@ $<

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
	$(mkinstalldirs) '$(DESTDIR)$(pgxsdir)/$(subdir)'

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

OBJS += regress_gp.o partindex.o workfile_mgr_test.o

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
	  $(INSTALL_DATA) $$file '$(DESTDIR)$(pkglibdir)/regress/'$$file; \
	done

installdirs-tests: installdirs
	$(mkinstalldirs)  $(patsubst $(srcdir)/%/,'$(DESTDIR)$(pkglibdir)/regress/%',$(sort $(dir $(regress_data_files))))


# Get some extra C modules from contrib/spi...

all: refint$(DLSUFFIX) autoinc$(DLSUFFIX) tablespace-setup includecheck hooktest bgworker_example$(DLSUFFIX)

refint$(DLSUFFIX): $(top_builddir)/contrib/spi/refint$(DLSUFFIX)
	cp $< $@

autoinc$(DLSUFFIX): $(top_builddir)/contrib/spi/autoinc$(DLSUFFIX)
	cp $< $@

$(top_builddir)/contrib/spi/refint$(DLSUFFIX): $(top_srcdir)/contrib/spi/refint.c
	$(MAKE) -C $(top_builddir)/contrib/spi refint$(DLSUFFIX)

$(top_builddir)/contrib/spi/autoinc$(DLSUFFIX): $(top_srcdir)/contrib/spi/autoinc.c
	$(MAKE) -C $(top_builddir)/contrib/spi autoinc$(DLSUFFIX)

bgworker_example$(DLSUFFIX): $(top_builddir)/contrib/bgworker_example/bgworker_example$(DLSUFFIX)
	cp $< $@

$(top_builddir)/contrib/bgworker_example/bgworker_example$(DLSUFFIX): $(top_srcdir)/contrib/bgworker_example/bgworker_example.c
	$(MAKE) -C $(top_builddir)/contrib/bgworker_example bgworker_example$(DLSUFFIX)

# Tablespace setup

all: tablespace-setup

.PHONY: tablespace-setup
tablespace-setup:
	rm -rf ./testtablespace
	mkdir ./testtablespace

# Check for include files that are not being shipped
.PHONY: includecheck
includecheck:
	$(srcdir)/checkinc.py

.PHONY: hooktest
hooktest:
	$(MAKE) -C $< $@

##
## Run tests
##

pg_regress_call = ./pg_regress --inputdir=$(srcdir) --dlpath=. --multibyte=$(MULTIBYTE) $(MAXCONNOPT) $(NOLOCALE) --init-file=$(srcdir)/init_file

# These are currently skipped because they don't work yet for segment wal replication
# segspace: cannot restart database at the moment
# filespace: segment wal replication does not handle filespaces yet
ifeq ($(enable_segwalrep), yes)
pg_regress_call := $(pg_regress_call) --exclude-tests="segspace_setup segspace segspace_cleanup filespace"
endif

check: all
	$(pg_regress_call) --temp-install=./tmp_check --top-builddir=$(top_builddir) --schedule=$(srcdir)/parallel_schedule 

installcheck: all
	$(pg_regress_call)  --psqldir=$(PSQLDIR) --schedule=$(srcdir)/serial_schedule 

installcheck-good: all twophase_pqexecparams hooktest
	if [ -z "$(INSTALLCHECK_GOOD_KERBEROS)" ]; then \
	$(pg_regress_call)  --psqldir=$(PSQLDIR) --schedule=$(srcdir)/parallel_schedule --schedule=$(srcdir)/greenplum_schedule --ao-dir=uao; \
	else \
	bash kerberos/setup_test.sh; \
	PGUSER="gpadmin/kerberos-test" $(pg_regress_call)  --psqldir=$(PSQLDIR) --schedule=$(srcdir)/parallel_schedule --schedule=$(srcdir)/greenplum_schedule --ao-dir=uao --host=`hostname`; \
	fi

testbouncer: all
	bash -c "bouncer/setup.sh"
	$(pg_regress_call)  --port=65432 --psqldir=$(PSQLDIR) --schedule=./minimal_schedule --srcdir=$(abs_srcdir) --host=`hostname`;
	bash -c "bouncer/stop.sh"

# old interfaces follow...

runcheck: check
runtest: installcheck
runtest-parallel: installcheck-parallel

bigtest: all
	$(pg_regress_call) --psqldir=$(PSQLDIR) --schedule=$(srcdir)/serial_schedule numeric_big 

bigcheck: all
	$(pg_regress_call) --temp-install=./tmp_check --top-builddir=$(top_builddir) --schedule=$(srcdir)/parallel_schedule $(MAXCONNOPT) numeric_big

bkuprestore: all
	$(pg_regress_call)  --psqldir=$(PSQLDIR) --schedule=$(srcdir)/bkuprestore_schedule --srcdir=$(abs_srcdir)

##
## Clean up
##

clean distclean maintainer-clean: clean-lib
# things built by `all' target
	$(MAKE) -C $(top_builddir)/contrib/spi clean
	$(MAKE) -C hooktest/ clean
	rm -f twophase_pqexecparams
# things created by dynamic configs
	find sql -type l | xargs rm -f
	find expected -type l | xargs rm -f
	rm -f $(OBJS) refint$(DLSUFFIX) autoinc$(DLSUFFIX) pg_regress_main.o pg_regress.o pg_regress$(X)
# things created by various check targets
	rm -f $(output_files) $(input_files)
	rm -rf testtablespace
	rm -rf results tmp_check log
	rm -f regression.diffs regression.out regress.out run_check.out
	rm -f data/pg_class32.data
	rm -f gmon.out
	rm -f $(srcdir)/sql/cppudf.sql

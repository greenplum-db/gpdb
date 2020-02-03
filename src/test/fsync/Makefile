MODULES=fsync_helper
PG_CONFIG=pg_config

subdir = src/test/fsync/
top_builddir = ../../..

include $(top_builddir)/src/Makefile.global

REGRESS = setup
ifeq ($(enable_faultinjector),yes)
REGRESS += bgwriter_checkpoint
REGRESS_OPTS = --load-extension=gp_inject_fault
endif

NO_PGXS = 1
include $(top_srcdir)/src/makefiles/pgxs.mk

# Ease the top-level Makefile's job a little bit, and install supporting
# libraries as part of installcheck.
installcheck: install

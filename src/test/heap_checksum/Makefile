MODULES=heap_checksum_helper
PG_CONFIG=pg_config

subdir = src/test/heap_checksum/
top_builddir = ../../..

include $(top_builddir)/src/Makefile.global

REGRESS = setup
REGRESS_OPTS = --init-file=../regress/init_file

ifeq ($(enable_faultinjector),yes)
REGRESS += heap_checksum_corruption
REGRESS_OPTS += --load-extension=gp_inject_fault
endif

NO_PGXS = 1
include $(top_srcdir)/src/makefiles/pgxs.mk

# Ease the top-level Makefile's job a little bit, and install supporting
# libraries as part of installcheck.
installcheck: install

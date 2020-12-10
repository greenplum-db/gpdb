MODULES=gplibpq
PG_CONFIG=pg_config

subdir = src/test/walrep
top_builddir = ../../..

include $(top_builddir)/src/Makefile.global

REGRESS = setup
REGRESS += replication_views_mirrored missing_xlog walreceiver generate_ao_xlog generate_aoco_xlog

NO_PGXS = 1
include $(top_srcdir)/src/makefiles/pgxs.mk

# Ease the top-level Makefile's job a little bit, and install supporting
# libraries as part of installcheck.
installcheck: install

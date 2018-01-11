MODULES=gplibpq
PG_CONFIG=pg_config

subdir = src/test/walrep/
top_builddir = ../../..

include $(top_builddir)/src/Makefile.global

WITH_MIRRORS ?= true
REGRESS = setup walreceiver

# These two tests can only run without mirrors due to limit of 1
# walsender-walreceiver connection only
ifeq ($(enable_segwalrep), yes)
ifeq ($(WITH_MIRRORS), false)
REGRESS += generate_ao_xlog generate_aoco_xlog replication_views_mirrorless
else
REGRESS += replication_views_mirrored missing_xlog
endif
endif
REGRESS_OPTS = --dbname="walrep_regression"

NO_PGXS = 1
include $(top_srcdir)/src/makefiles/pgxs.mk

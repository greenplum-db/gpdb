EXTENSION = gp_replica_check
DATA = gp_replica_check--0.0.1.sql
MODULES = gp_replica_check
SCRIPTS = gp_replica_check.py

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/gp_replica_check
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

# GPDB_FEATURE_NOT_SUPPORTED: Enable btree, gin and gist as physical
# and logical order is equivalent now. Also, enable brin, spgist,
# bitmap. Basically, "all" option should be functional for this tool.
installcheck: install
	gp_replica_check.py -r="heap, ao_row, ao_column"

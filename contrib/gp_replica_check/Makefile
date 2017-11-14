EXTENSION = gp_replica_check
DATA = gp_replica_check--0.0.1.sql
MODULES = gp_replica_check
SCRIPTS = gp_replica_check.py

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

# For now run only for HEAP as for other object types like Sequence, AO, btree
# there are still known differences to be fixed.
installcheck: install
	gp_replica_check.py -r="heap, sequence"

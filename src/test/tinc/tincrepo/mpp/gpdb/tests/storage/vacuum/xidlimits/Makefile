MODULES=xidhelper
PG_CONFIG=pg_config

# Use what ever compiler is available on the system instead of 
# the one reported by pg_config since it may not be available.
override CC:=gcc
override CFLAGS:=-m64

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

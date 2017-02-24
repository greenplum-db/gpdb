MODULES = lockrelease
DATA_built = install.sql

override CC:=gcc
override CFLAGS:=-m64

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

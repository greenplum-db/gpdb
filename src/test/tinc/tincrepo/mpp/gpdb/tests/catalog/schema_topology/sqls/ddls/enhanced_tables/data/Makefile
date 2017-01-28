MODULES = tabfunc_demo

PG_CONFIG = pg_config
override CFLAGS:=-m64
override CPPFLAGS=-D_GNU_SOURCE
override CC:=gcc
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)


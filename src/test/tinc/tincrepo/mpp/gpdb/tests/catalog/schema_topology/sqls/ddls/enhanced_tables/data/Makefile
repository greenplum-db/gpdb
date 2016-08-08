MODULES = tabfunc_demo

PG_CONFIG = pg_config
override CFLAGS = $(CFLAGS_SL)
override CPPFLAGS=-D_GNU_SOURCE
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)


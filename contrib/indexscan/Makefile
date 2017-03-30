MODULES = indexscan
DATA_built = indexscan.sql
REGRESS = create_function read_heap_index read_ao_index read_co_index drop_function

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/indexscan
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif


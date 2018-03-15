MODULE_big = query_info_hook_test
OBJS = query_info_hook_test.o

REGRESS = query_info_hook_test

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = src/test/regress/query_info_hook_test
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

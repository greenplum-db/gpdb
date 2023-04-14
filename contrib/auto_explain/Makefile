# contrib/auto_explain/Makefile

MODULE_big = auto_explain
OBJS = auto_explain.o $(WIN32RES)
PGFILEDESC = "auto_explain - logging facility for execution plans"

REGRESS = auto_explain bfv_preload_auto_explain memory_leaks
REGRESS_OPTS = --init-file=init_file

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/auto_explain
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

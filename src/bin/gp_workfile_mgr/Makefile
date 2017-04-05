top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

MODULE_big = gp_workfile_mgr
OBJS       = gp_workfile_cache_stats.o

ifdef USE_PGXS
PGXS := $(shell pg_config --pgxs)
include $(PGXS)
else
subdir = src/bin/gp_workfile_mgr
include $(top_srcdir)/contrib/contrib-global.mk
endif

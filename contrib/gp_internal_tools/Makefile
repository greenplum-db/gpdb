MODULES    = gp_ao_co_diagnostics gp_workfile_mgr gp_session_state_memory_stats gp_instrument_shmem
DATA       = gp_session_state.sql uninstall_gp_session_state.sql

PG_CPPFLAGS = -I$(libpq_srcdir)

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/gp_diagnostic_libraries
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

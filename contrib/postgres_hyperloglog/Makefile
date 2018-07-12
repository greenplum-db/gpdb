MODULE_big = hyperloglog_counter
OBJS = src/hyperloglog_counter.o 
EXTENSION = hyperloglog_counter
DATA = sql/hyperloglog_counter--2.0.0.sql

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/postgres_hyperloglog
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

all: hyperloglog_counter.so

hyperloglog_counter.so: $(OBJS)

%.o : src/%.c

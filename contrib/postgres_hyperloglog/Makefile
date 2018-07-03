MODULE_big = hyperloglog_counter
OBJS = src/hyperloglog_counter.o src/hyperloglog.o src/upgrade.o src/hllutils.o src/encoding.o
EXTENSION = hyperloglog_counter
DATA = sql/hyperloglog_counter--2.0.0.sql

TEST_VERSION := $(shell psql -tAc "select case when lower(version()) like '%greenplum%' then 'gp' else 'pg' end")
OUT_DIR = test/expected
SQL_DIR = test/sql
PSQL = psql
PSQLOPTS  = -X --echo-all -P null=NULL
PGOPTIONS = --client-min-messages=warning

GLOBAL_BASE_TEST = aggs set_ops operators compression
ifeq ($(TEST_VERSION),gp)
  BASE_TEST = gp_base $(GLOBAL_BASE_TEST) gp_persistence gp_update
else
  BASE_TEST = base $(GLOBAL_BASE_TEST) update
endif

TEST         = $(foreach test,$(BASE_TEST),$(SQL_DIR)/$(test).out)
TESTS        = $(foreach test,$(BASE_TEST),$(SQL_DIR)/$(test).sql)
REGRESS      = $(patsubst $(SQL_DIR)%,%,$(TESTS))
REGRESS_OPTS = -X --echo-all -P null=NULL

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

tests: clean_test $(TEST)
	@find . -maxdepth 1 -name '*.diff' -print -quit > failures
	@if test -s failures; then \
        echo ERROR: `ls -1 $(SQL_DIR)/*.diff | wc -l` / `ls -1 $(SQL_DIR)/*.sql | wc -l` tests failed; \
        echo; \
        cat *.diff; \
        exit 1; \
    else \
        rm failures; \
        echo `ls -1 $(SQL_DIR)/*.out | wc -l` / `ls -1 $(SQL_DIR)/*.sql | wc -l` tests passed; \
    fi

%.out:
	@echo $*
	@if test -f ../testdata/$*.csv; then \
      PGOPTIONS=$(PGOPTIONS) $(PSQL) $(PSQLOPTS) -f $*.sql < ../testdata/$*.csv > $*.out 2>&1; \
    else \
      PGOPTIONS=$(PGOPTIONS) $(PSQL) $(PSQLOPTS) -f $*.sql >> $*.out 2>&1; \
    fi
	@diff -u $*.ref $*.out >> $*.diff || status=1
	@if test -s $*.diff; then \
        echo " .. FAIL"; \
    else \
        echo " .. PASS"; \
        rm -f $*.diff; \
    fi

clean_test:
	rm -f $(SQL_DIR)/*.out $(SQL_DIR)/*.diff

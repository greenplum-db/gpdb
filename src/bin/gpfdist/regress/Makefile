top_builddir = $(shell cd ../../../.. && pwd)
include $(top_builddir)/src/Makefile.global

default: installcheck

REGRESS = exttab1 custom_format
PSQLDIR = $(prefix)/bin

installcheck:
	$(top_builddir)/src/test/regress/pg_regress --psqldir=$(PSQLDIR) $(REGRESS)

clean:
	rm -rf regression.* sql results expected

distclean: clean

.PHONY: clean distclean



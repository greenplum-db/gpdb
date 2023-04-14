MODULE_big = fixedwidth
OBJS       = fixedwidth.o
REGRESS = readable_query02 readable_query03 readable_query04 readable_query05 readable_query10 readable_query11 readable_query12 readable_query13 readable_query14 readable_query15 readable_query16 readable_query17 readable_query18 readable_query19 readable_query20 readable_query21 readable_query22 readable_query23 readable_query24 readable_query25 readable_query26 readable_query27 readable_query28 readable_query29 readable_query30 readable_query31 readable_query32 readable_query33 readable_query34 readable_query35 readable_query36

PG_CPPFLAGS = -I$(libpq_srcdir)
PG_LIBS = $(libpq_pgport)

ifdef USE_PGXS
PGXS := $(shell pg_config --pgxs)
include $(PGXS)
else
subdir = contrib/formatter_fixedwidth
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

.PHONY: install-data
install-data:
	$(INSTALL_DATA) fixedwidth.sql '$(DESTDIR)$(datadir)/cdb_init.d/fixedwidth.sql'

install: install-data

.PHONY: uninstall-data
uninstall-data:
	rm -f '$(DESTDIR)$(datadir)/cdb_init.d/fixedwidth.sql'

uninstall: uninstall-data

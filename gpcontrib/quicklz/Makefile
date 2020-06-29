# Makefile for QuickLZ compressor

MODULE_big = quicklz_compressor
OBJS = quicklz_compression.o
CFLAGS_SL += -lquicklz
LDFLAGS_SL += -lquicklz

REGRESS = AORO_quicklz AOCO_quicklz quicklz_column_compression compression_quicklz

ifdef USE_PGXS
  PGXS := $(shell pg_config --pgxs)
  include $(PGXS)
else
  top_builddir = ../..
  include $(top_builddir)/src/Makefile.global
  include $(top_srcdir)/contrib/contrib-global.mk
endif

# when libquicklz is vendored, it is copied in to GPHOME/lib
# the quicklz_compressor extension is located in GPHOME/lib/postgresql/
ifdef QUICKLZ_LDFLAGS
  LDFLAGS += $(QUICKLZ_LDFLAGS)
endif

# Install into cdb_init.d, so that the catalog changes performed by initdb,
# and the compressor is available in all databases.
.PHONY: install-data
install-data:
	$(INSTALL_DATA) quicklz_compressor.sql '$(DESTDIR)$(datadir)/cdb_init.d/quicklz_compressor.sql'

install: install-data

.PHONY: uninstall-data
uninstall-data:
	rm -f '$(DESTDIR)$(datadir)/cdb_init.d/quicklz_compressor.sql'

uninstall: uninstall-data

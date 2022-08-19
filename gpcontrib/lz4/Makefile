PG_CONFIG = pg_config

MODULE_big = gp_lz4_compression
OBJS = lz4_compression.o
CFLAGS_SL += -llz4
LDFLAGS_SL += -llz4

REGRESS = lz4_column_compression compression_lz4 AOCO_lz4 AORO_lz4

ifdef USE_PGXS
  PGXS := $(shell $(PG_CONFIG) --pgxs)
  include $(PGXS)
else
  top_builddir = ../..
  include $(top_builddir)/src/Makefile.global
  include $(top_srcdir)/contrib/contrib-global.mk
endif

# Install into cdb_init.d, so that the catalog changes performed by initdb,
# and the compressor is available in all databases.
.PHONY: install-data
install-data:
	$(INSTALL_DATA) lz4_compression.sql '$(DESTDIR)$(datadir)/cdb_init.d/lz4_compression.sql'

install: install-data

.PHONY: uninstall-data

uninstall-data:
	rm -f '$(DESTDIR)$(datadir)/cdb_init.d/lz4_compression.sql'

uninstall: uninstall-data

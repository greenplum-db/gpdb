PG_CONFIG = pg_config

MODULE_big = gp_zstd_compression
OBJS = zstd_compression.o
CFLAGS_SL += -lzstd
LDFLAGS_SL += -lzstd

REGRESS = zstd_column_compression compression_zstd zstd_abort_leak AOCO_zstd AORO_zstd

ifdef USE_PGXS
  PGXS := $(shell pg_config --pgxs)
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
	$(INSTALL_DATA) zstd_compression.sql '$(DESTDIR)$(datadir)/cdb_init.d/zstd_compression.sql'

install: install-data

.PHONY: uninstall-data

uninstall-data:
	rm -f '$(DESTDIR)$(datadir)/cdb_init.d/zstd_compression.sql'

uninstall: uninstall-data


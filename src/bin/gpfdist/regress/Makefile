top_builddir = $(shell cd ../../../.. && pwd)
include $(top_builddir)/src/Makefile.global

default: installcheck

REGRESS = exttab1 custom_format gpfdist2
ifeq ($(with_openssl),yes)
	REGRESS += gpfdist_ssl
endif

PSQLDIR = $(prefix)/bin
REGRESS_OPTS = --init-file=init_file

installcheck:
	@-cp -rf $(MASTER_DATA_DIRECTORY)/gpfdists data/gpfdist_ssl/certs_matching
	$(top_builddir)/src/test/regress/pg_regress --psqldir=$(PSQLDIR) --dbname=gpfdist_regression $(REGRESS) $(REGRESS_OPTS)

clean:
	rm -rf regression.* sql results expected

distclean: clean

.PHONY: installcheck clean distclean

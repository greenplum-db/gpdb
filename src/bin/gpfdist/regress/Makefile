top_builddir = $(shell cd ../../../.. && pwd)
include $(top_builddir)/src/Makefile.global

default: installcheck

REGRESS = exttab1 custom_format gpfdist2

ifeq ($(enable_gpfdist),yes)
#ifeq ($(with_openssl),yes)
#	REGRESS += gpfdist_ssl
#endif
endif

PSQLDIR = $(prefix)/bin
REGRESS_OPTS = --init-file=init_file

installcheck: watchdog
ifeq ($(enable_gpfdist),yes)
#ifeq ($(with_openssl),yes)
#	cp -rf $(MASTER_DATA_DIRECTORY)/gpfdists data/gpfdist_ssl/certs_matching
#	cp data/gpfdist_ssl/certs_matching/root.crt data/gpfdist_ssl/certs_not_matching
#endif
endif
	$(top_builddir)/src/test/regress/pg_regress --psqldir=$(PSQLDIR) --dbname=gpfdist_regression $(REGRESS) $(REGRESS_OPTS)

watchdog:
	sh test_watchdog.sh

clean:
	rm -rf regression.* sql results expected

distclean: clean

.PHONY: installcheck clean distclean

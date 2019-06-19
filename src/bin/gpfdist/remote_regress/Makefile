top_builddir = $(shell cd ../../../.. && pwd)
include $(top_builddir)/src/Makefile.global

default: installcheck

REGRESS =

ifeq ($(enable_gpfdist),yes)
ifeq ($(with_openssl),yes)
	REGRESS += gpfdist_ssl
endif
endif

PSQLDIR = $(prefix)/bin
REGRESS_OPTS = --init-file=init_file

pre_installcheck:
	rm -rf data
	mkdir data
	cp -rf ../regress/data/* data/
ifeq ($(enable_gpfdist),yes)
ifeq ($(with_openssl),yes)
	cp -rf $(MASTER_DATA_DIRECTORY)/gpfdists data/gpfdist_ssl/certs_matching
	cp data/gpfdist_ssl/certs_matching/root.crt data/gpfdist_ssl/certs_not_matching
	scp -r -P ${REMOTE_PORT} ./data/gpfdist_ssl  ${REMOTE_USER}@${REMOTE_HOST}:
endif
endif

installcheck_win: pre_installcheck
	rm -rf start_gpfdist_remote
	mkdir start_gpfdist_remote
	cp -rf start_gpfdist_remote_win/* start_gpfdist_remote
	scp -P ${REMOTE_PORT} start_gpfdist_remote/*.bat  ${REMOTE_USER}@${REMOTE_HOST}:
	$(top_builddir)/src/test/regress/pg_regress --psqldir=$(PSQLDIR) --dbname=gpfdist_regression $(REGRESS) $(REGRESS_OPTS)

clean:
	rm -rf regression.* sql results expected

distclean: clean

.PHONY: installcheck clean distclean

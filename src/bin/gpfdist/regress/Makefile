top_builddir = $(shell cd ../../../.. && pwd)
include $(top_builddir)/src/Makefile.global

default: installcheck

REGRESS = exttab1 custom_format gpfdist2 gpfdist_path

ifeq ($(enable_gpfdist),yes)
ifeq ($(with_openssl),yes)
	REGRESS += gpfdist_ssl gpfdists_multiCA
endif
endif

ifeq ($(with_zstd),yes)
	REGRESS += gpfdist2_compress
endif

REGRESS_OPTS = --init-file=init_file

installcheck: watchdog ipv4v6_ports
ifeq ($(enable_gpfdist),yes)
ifeq ($(with_openssl),yes)
	rm -rf data/gpfdist_ssl/certs_matching
	mkdir data/gpfdist_ssl/certs_matching
	cp -rf $(MASTER_DATA_DIRECTORY)/gpfdists/* data/gpfdist_ssl/certs_matching
	# for verify_gpfdists_cert=off
	cp data/gpfdist_ssl/certs_matching/root.crt data/gpfdist_ssl/certs_not_matching
endif
endif
ifeq ($(with_zstd),yes)
	rm -rf data/gpfdist2/lineitem.tbl.long
	touch data/gpfdist2/lineitem.tbl.long
	for name in `seq 1 1000`; \
	do \
		head -100 data/gpfdist2/lineitem.tbl >> data/gpfdist2/lineitem.tbl.long; \
	done  
endif
	$(top_builddir)/src/test/regress/pg_regress --dbname=gpfdist_regression $(REGRESS) $(REGRESS_OPTS)

watchdog:
	sh test_watchdog.sh

ipv4v6_ports:
	./test_ipv4v6_port.sh

clean:
	rm -rf regression.* sql results expected

distclean: clean

.PHONY: installcheck clean distclean

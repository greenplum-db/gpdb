top_builddir = $(shell cd ../../../.. && pwd)
include $(top_builddir)/src/Makefile.global

default: installcheck

REGRESS = exttab1 custom_format gpfdist2 gpfdist_path

# Get the OpenSSL version
OPENSSL_VERSION := $(shell openssl version 2>/dev/null)
$(info $(OPENSSL_VERSION))

# Extract the major, minor, and fix version numbers
OPENSSL_MAJOR_VERSION := $(shell echo '$(OPENSSL_VERSION)' | cut -d' ' -f2 | cut -d. -f1)
OPENSSL_MINOR_VERSION := $(shell echo '$(OPENSSL_VERSION)' | cut -d' ' -f2 | cut -d. -f2)
OPENSSL_FIX_VERSION := $(shell echo '$(OPENSSL_VERSION)' | cut -d' ' -f2 | cut -d. -f3 | sed 's/[^0-9]*//g')

ifeq ($(enable_gpfdist),yes)
ifeq ($(with_openssl),yes)	
    ifeq (1,$(shell [ $(OPENSSL_MAJOR_VERSION) -gt 1 ] || ( [ $(OPENSSL_MAJOR_VERSION) -eq 1 ] && [ $(OPENSSL_MINOR_VERSION) -ge 1 ] && [ $(OPENSSL_FIX_VERSION) -ge 1 ] ) && echo 1 ))
        REGRESS += gpfdist_ssl gpfdists_multiCA
    else
        REGRESS += gpfdist_ssl gpfdists_multiCA gpfdist_old_ssl
    endif
endif
endif
$(info $(REGRESS))

PSQLDIR = $(prefix)/bin
ifeq ($(with_zstd),yes)
	REGRESS += gpfdist2_compress
endif

REGRESS_OPTS = --init-file=init_file

installcheck: watchdog ipv4v6_ports
ifeq ($(enable_gpfdist),yes)
ifeq ($(with_openssl),yes)
	cp -rf $(MASTER_DATA_DIRECTORY)/gpfdists data/gpfdist_ssl/certs_matching
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
	$(top_builddir)/src/test/regress/pg_regress --psqldir=$(PSQLDIR) --dbname=gpfdist_regression $(REGRESS) $(REGRESS_OPTS)

watchdog:
	sh test_watchdog.sh

ipv4v6_ports:
	./test_ipv4v6_port.sh

clean:
	rm -rf regression.* sql results expected

distclean: clean

.PHONY: installcheck clean distclean

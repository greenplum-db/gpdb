# src/test/modules/connection/Makefile

subdir = src/test/modules/connection
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

export with_openssl

check:
	$(prove_check)

installcheck:
	$(prove_installcheck)

clean distclean maintainer-clean:
	rm -rf tmp_check

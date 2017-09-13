EXTENSION = pxf
DATA = pxf--1.0.sql
MODULE_big = pxf
OBJS       = src/pxfprotocol.o src/pxfbridge.o src/pxfuriparser.o src/libchurl.o src/pxfutils.o src/pxfheaders.o src/pxffragment.o src/gpdbwritableformatter.o
REGRESS    = setup pxf pxfinvalid

ifdef USE_PGXS
	PGXS := $(shell pg_config --pgxs)
include $(PGXS)
else
	top_builddir = ../../..
	include $(top_builddir)/src/Makefile.global
	include $(top_srcdir)/contrib/contrib-global.mk
endif

CURL_DIR = /usr/local/opt/curl

SHLIB_LINK += -lcurl

unittest-check:
	$(MAKE) -C test check

unittest-clean:
	$(MAKE) -C test clean

install: copy-curl

# for Dev env on MacOS copy updated curl to gpdb install location
copy-curl:
    ifeq ($(PORTNAME), darwin)
        ifeq "$(wildcard $(CURL_DIR))" ""
			$(error System curl incompatible. Please run "brew install curl")
        endif
		cp -rf $(CURL_DIR)/lib/lib* ${libdir}
		(cd ${libdir} && rm -rf libcurl.dylib && ln -s libcurl.4.dylib libcurl.dylib)
    endif

uninstall: remove-curl

# for Dev env on MacOS remove updated curl from gpdb install location
remove-curl:
    ifeq ($(PORTNAME), darwin)
		rm -rf ${libdir}/libcurl*
    endif

clean: unittest-clean

.PHONY: unittest-check unittest-clean

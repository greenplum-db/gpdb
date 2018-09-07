# protect the default target for this file from the targets in Makefile.global
# and Makefile.thirdparty
default: all

BLD_TOP=..
include $(BLD_TOP)/gpAux/Makefile.windows

top_builddir = ..
include $(top_builddir)/src/Makefile.global

.PHONY: all install clean distclean gpcloud

#
# targets built via top_builddir/GNUmakefile
#

all: gpcloud

gpcloud:
	@if [ "$(enable_gpcloud)" = "yes" ]; then \
		echo "gpcloud enabled"; \
		$(MAKE) -C gpcloud && \
		$(MAKE) -C gpcloud/bin/gpcheckcloud; \
	fi

install:
	@if [ "$(enable_gpcloud)" = "yes" ]; then \
		$(MAKE) -C gpcloud install && \
		$(MAKE) -C gpcloud/bin/gpcheckcloud install; \
	fi

clean:
	if [ "$(enable_gpcloud)" = "yes" ]; then $(MAKE) -C gpcloud cleanall; fi

distclean:
	if [ "$(enable_gpcloud)" = "yes" ]; then $(MAKE) -C gpcloud distclean; fi


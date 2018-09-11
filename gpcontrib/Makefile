# protect the default target for this file from the targets in Makefile.global
# and Makefile.thirdparty
default: all

BLD_TOP=..
include $(BLD_TOP)/gpAux/Makefile.windows

top_builddir = ..
include $(top_builddir)/src/Makefile.global

.PHONY: all install clean distclean gpcloud pxf mapreduce

#
# targets built via top_builddir/GNUmakefile
#

$(call recurse,all install clean distclean,\
			   gp_sparse_vector \
			   gp_distribution_policy \
			   gp_internal_tools \
			   gp_inject_fault)

all: gpcloud pxf mapreduce

gpcloud:
	@if [ "$(enable_gpcloud)" = "yes" ]; then \
		echo "gpcloud enabled"; \
		$(MAKE) -C gpcloud && \
		$(MAKE) -C gpcloud/bin/gpcheckcloud; \
	fi

pxf:
	@if [ "$(enable_pxf)" = "yes" ]; then \
		echo "pxf enabled"; \
		$(MAKE) -C pxf; \
	fi

mapreduce:
	@if [ "$(enable_mapreduce)" = "yes" ]; then \
		echo "gpmapreduce enabled"; \
		$(MAKE) -C gpmapreduce; \
	fi

install:
	@if [ "$(enable_gpcloud)" = "yes" ]; then \
		$(MAKE) -C gpcloud install && \
		$(MAKE) -C gpcloud/bin/gpcheckcloud install; \
	fi
	@if [ "$(enable_mapreduce)" = "yes" ]; then \
		$(MAKE) -C gpmapreduce install; \
	fi
	if [ "$(enable_pxf)" = "yes" ]; then $(MAKE) -C pxf install; fi

clean:
	if [ "$(enable_mapreduce)" = "yes" ]; then $(MAKE) -C gpmapreduce clean; fi
	if [ "$(enable_gpcloud)" = "yes" ]; then $(MAKE) -C gpcloud cleanall; fi
	if [ "$(enable_pxf)" = "yes" ]; then $(MAKE) -C pxf clean; fi

distclean:
	if [ "$(enable_mapreduce)" = "yes" ]; then $(MAKE) -C gpmapreduce distclean; fi
	if [ "$(enable_gpcloud)" = "yes" ]; then $(MAKE) -C gpcloud distclean; fi
	if [ "$(enable_pxf)" = "yes" ]; then $(MAKE) -C pxf distclean; fi

installcheck:
	if [ "$(enable_mapreduce)" = "yes" ]; then \
		$(MAKE) -C gpmapreduce installcheck; \
	fi

unittest-check:
	@if [ "$(enable_pxf)" = "yes" ]; then \
		echo "pxf enabled"; \
		$(MAKE) -C pxf unittest-check; \
	fi


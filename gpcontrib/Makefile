# protect the default target for this file from the targets in Makefile.global
# and Makefile.thirdparty
default: all

BLD_TOP=..

GPAUX_DIR=$(BLD_TOP)/gpAux/

ext_dir=$(BLD_TOP)/gpAux/ext

top_builddir = ..
include $(top_builddir)/src/Makefile.global

.PHONY: all install clean distclean gpcloud pxf mapreduce gphdfs mkgphdfs

#
# targets built via top_builddir/GNUmakefile
#
recurse_targets = gp_sparse_vector \
               gp_distribution_policy \
               gp_internal_tools \
               gp_inject_fault
ifeq "$(with_zstd)" "yes"
	recurse_targets += zstd
endif
$(call recurse,all install clean distclean, $(recurse_targets))

all: gpcloud pxf mapreduce gphdfs

gpcloud:
	@if [ "$(enable_gpcloud)" = "yes" ]; then \
		$(MAKE) -C gpcloud && \
		$(MAKE) -C gpcloud/bin/gpcheckcloud; \
	fi

pxf:
	@if [ "$(enable_pxf)" = "yes" ]; then \
		$(MAKE) -C pxf; \
	fi

mapreduce:
	@if [ "$(enable_mapreduce)" = "yes" ]; then \
		$(MAKE) -C gpmapreduce; \
	fi

gphdfs:
	@if [ "$(enable_gphdfs)" = "yes" ]; then \
		$(MAKE) -C gphdfs; \
	fi

mkgphdfs:
	PATH=$(INSTLOC)/bin:$(PATH) $(MAKE) -C gphdfs USE_PGXS=1 install \
		JAVA_HOME=${JAVA_HOME}

install:
	@if [ "$(enable_gpcloud)" = "yes" ]; then \
		$(MAKE) -C gpcloud install && \
		$(MAKE) -C gpcloud/bin/gpcheckcloud install; \
	fi
	@if [ "$(enable_mapreduce)" = "yes" ]; then \
		$(MAKE) -C gpmapreduce install; \
	fi
	@if [ "$(enable_pxf)" = "yes" ]; then \
		$(MAKE) -C pxf install; \
	fi
	@if [ "$(enable_gphdfs)" = "yes" ]; then \
		$(MAKE) -C gphdfs docdir=$(INSTLOC)/docs install; \
	fi

clean:
	if [ "$(enable_mapreduce)" = "yes" ]; then $(MAKE) -C gpmapreduce clean; fi
	if [ "$(enable_gpcloud)" = "yes" ]; then $(MAKE) -C gpcloud cleanall; fi
	if [ "$(enable_pxf)" = "yes" ]; then $(MAKE) -C pxf clean; fi
	if [ "$(enable_gphdfs)" = "yes" ]; then $(MAKE) -C gphdfs clean; fi

distclean:
	if [ "$(enable_mapreduce)" = "yes" ]; then $(MAKE) -C gpmapreduce distclean; fi
	if [ "$(enable_gpcloud)" = "yes" ]; then $(MAKE) -C gpcloud distclean; fi
	if [ "$(enable_pxf)" = "yes" ]; then $(MAKE) -C pxf distclean; fi
	if [ "$(enable_gphdfs)" = "yes" ]; then $(MAKE) -C gphdfs distclean; fi

installcheck:
	$(MAKE) -C gp_internal_tools installcheck
	if [ "$(enable_mapreduce)" = "yes" ]; then \
		$(MAKE) -C gpmapreduce installcheck; \
	fi
ifeq "$(with_zstd)" "yes"
	$(MAKE) -C zstd installcheck
endif
	if [ -d "$(ext_dir)" ]; then \
		PATH=$(INSTLOC)/bin:$(PATH) $(MAKE) -C gphdfs installcheck USE_PGXS=1 ; \
	fi

unittest-check:
	@if [ "$(enable_pxf)" = "yes" ]; then \
		$(MAKE) -C pxf unittest-check; \
	fi

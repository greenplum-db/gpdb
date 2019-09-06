# protect the default target for this file from the targets in Makefile.global
# and Makefile.thirdparty
default: all

top_builddir = ..
include $(top_builddir)/src/Makefile.global

.PHONY: all install clean distclean gpcloud pxf mapreduce

#
# targets built via top_builddir/GNUmakefile
#
recurse_targets = ""

ifeq "$(enable_debug_extensions)" "yes"
	recurse_targets = gp_sparse_vector \
               gp_distribution_policy \
               gp_internal_tools \
               gp_debug_numsegments \
               gp_inject_fault \
               gp_replica_check
else
	recurse_targets = gp_sparse_vector \
               gp_distribution_policy \
               gp_internal_tools
endif

ifeq "$(with_zstd)" "yes"
	recurse_targets += zstd
endif
ifeq "$(with_quicklz)" "yes"
	recurse_targets += quicklz
endif
$(call recurse,all install clean distclean, $(recurse_targets))

all: gpcloud pxf mapreduce orafce

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

orafce:
	@if [ "$(enable_orafce)" = "yes" ]; then \
		$(MAKE) -C orafce NO_PGXS=true; \
	fi

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
	@if [ "$(enable_orafce)" = "yes" ]; then \
		$(MAKE) -C orafce NO_PGXS=true install; \
	fi

clean:
	if [ "$(enable_mapreduce)" = "yes" ]; then $(MAKE) -C gpmapreduce clean; fi
	if [ "$(enable_gpcloud)" = "yes" ]; then $(MAKE) -C gpcloud cleanall; fi
	if [ "$(enable_pxf)" = "yes" ]; then $(MAKE) -C pxf clean; fi
	if [ "${enable_orafce}" = "yes" ]; then $(MAKE) -C orafce NO_PGXS=true clean; fi


distclean:
	if [ "$(enable_mapreduce)" = "yes" ]; then $(MAKE) -C gpmapreduce distclean; fi
	if [ "$(enable_gpcloud)" = "yes" ]; then $(MAKE) -C gpcloud distclean; fi
	if [ "$(enable_pxf)" = "yes" ]; then $(MAKE) -C pxf distclean; fi
	if [ "${enable_orafce}" = "yes" ]; then $(MAKE) -C orafce NO_PGXS=true distclean; fi

installcheck:
	$(MAKE) -C gp_internal_tools installcheck
	if [ "$(enable_mapreduce)" = "yes" ]; then \
		$(MAKE) -C gpmapreduce installcheck; \
	fi
	if [ "$(enable_orafce)" = "yes" ]; then $(MAKE) -C orafce installcheck; fi

ifeq "$(with_zstd)" "yes"
	$(MAKE) -C zstd installcheck
endif

ifeq "$(with_quicklz)" "yes"
	$(MAKE) -C quicklz installcheck
endif

unittest-check:
	@if [ "$(enable_pxf)" = "yes" ]; then \
		$(MAKE) -C pxf unittest-check; \
	fi

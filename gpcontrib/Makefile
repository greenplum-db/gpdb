# protect the default target for this file from the targets in Makefile.global
# and Makefile.thirdparty
default: all

top_builddir = ..
include $(top_builddir)/src/Makefile.global

.PHONY: all install clean distclean gpcloud

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
               gp_exttable_fdw \
               gp_legacy_string_agg \
               gp_replica_check \
               gp_toolkit
else
	recurse_targets = gp_sparse_vector \
               gp_distribution_policy \
               gp_internal_tools \
               gp_legacy_string_agg \
               gp_exttable_fdw \
               gp_toolkit
endif

ifeq "$(with_zstd)" "yes"
	recurse_targets += zstd
endif
$(call recurse,all install clean distclean, $(recurse_targets))

all: gpcloud orafce

gpcloud:
	@if [ "$(enable_gpcloud)" = "yes" ]; then \
		$(MAKE) -C gpcloud && \
		$(MAKE) -C gpcloud/bin/gpcheckcloud; \
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
	@if [ "$(enable_orafce)" = "yes" ]; then \
		$(MAKE) -C orafce NO_PGXS=true install; \
	fi

clean:
	if [ "$(enable_gpcloud)" = "yes" ]; then $(MAKE) -C gpcloud cleanall; fi
	if [ "${enable_orafce}" = "yes" ]; then $(MAKE) -C orafce NO_PGXS=true clean; fi


distclean:
	if [ "$(enable_gpcloud)" = "yes" ]; then $(MAKE) -C gpcloud distclean; fi
	if [ "${enable_orafce}" = "yes" ]; then $(MAKE) -C orafce NO_PGXS=true distclean; fi

installcheck:
	$(MAKE) -C gp_internal_tools installcheck
	if [ "$(enable_orafce)" = "yes" ]; then $(MAKE) -C orafce installcheck; fi
	if [ "$(with_zstd)" = "yes" ]; then $(MAKE) -C zstd installcheck; fi
	$(MAKE) -C gp_sparse_vector installcheck
	$(MAKE) -C gp_exttable_fdw installcheck
	$(MAKE) -C gp_toolkit installcheck

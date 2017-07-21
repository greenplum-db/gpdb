# include the top level Makefile.global to ensure that we can read the configuration variable
top_builddir = ../..
include $(top_builddir)/src/Makefile.global

ifeq ($(enable_go_utils),yes)
all:
	$(MAKE) -C src/gp_upgrade $@

clean distclean:
	$(MAKE) -C src/gp_upgrade clean

install:
	$(MAKE) -C src/gp_upgrade $@

# do nothing if global config is not enabled
else
all:
	@echo "Configuration not enabled for go-utils."

endif

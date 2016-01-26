#-------------------------------------------------------------------------
#
# Makefile for src/backend/gppc library
#
#
#-------------------------------------------------------------------------

# GPPC is ready to build against installed backend, so that we can try it
# against different backend releases without switching source tree.
# This means GPPC should build and work with older releases without
# source modified and we need to keep it identical between different
# backend release branches.
# We achieve to build with another backend release by PGXS. Everything
# GPPC needs (i.e. headers and makefiles) are installed along with pg_config.


subdir = src/backend/gppc
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

# Mac OS needs dynamic_lookup for undefined symbols when using -dynamiclib
ifeq ($(PORTNAME), darwin)
  override LINK.shared += -undefined dynamic_lookup
endif

# shared library parameters
NAME = gppc
SO_MAJOR_VERSION = 1
SO_MINOR_VERSION = 2

override CPPFLAGS := -I$(includedir_server) -I$(includedir_internal) $(CPPFLAGS)
OBJS = gppc.o

all: all-lib
include $(top_srcdir)/src/Makefile.shlib 	# Shared library stuff

override pkglibdir := $(libdir)

install: install-lib

installcheck: install
	$(MAKE) -C test $@

uninstall: uninstall-lib

clean distclean: clean-lib
	rm -f $(OBJS)
 
maintainer-clean: distclean

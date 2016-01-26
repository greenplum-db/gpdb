# Makefile for PL/R

ifneq (,${R_HOME})

r_libdir1x  = ${R_HOME}/bin
r_libdir2x  = ${R_HOME}/lib
r_include   = ${R_HOME}/include
r_home_default = ${R_HOME}

MODULE_big	:= plr
PG_CPPFLAGS	+= -I$(r_include)
SRCS		+= plr.c pg_conversion.c pg_backend_support.c pg_userfuncs.c pg_rsupport.c
OBJS		:= $(SRCS:.c=.o)
SHLIB_LINK	+= -L$(r_libdir1x) -L$(r_libdir2x) -lR

DATA_built	:= plr.sql 
REGRESS		:= plr
EXTRA_CLEAN	:= doc/HTML.index

ifdef USE_PGXS
ifndef PG_CONFIG
PG_CONFIG := pg_config
endif
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/plr
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

ifeq ($(PORTNAME), darwin)
	DYSUFFIX = dylib
	DLPREFIX = libR
else
	ifeq ($(PORTNAME), win32)
		DLPREFIX = R
	else
		DLPREFIX = libR
	endif
endif

# we can only build PL/R if libR is available
# Since there is no official way to determine this,
# we see if there is a file that is named like a shared library.
ifneq ($(PORTNAME), darwin)
	ifneq (,$(wildcard $(r_libdir1x)/$(DLPREFIX)*$(DLSUFFIX)*)$(wildcard $(r_libdir2x)/$(DLPREFIX)*$(DLSUFFIX)*))
		shared_libr = yes;
	endif
else
	ifneq (,$(wildcard $(r_libdir1x)/$(DLPREFIX)*$(DYSUFFIX)*)$(wildcard $(r_libdir2x)/$(DLPREFIX)*$(DYSUFFIX)*))
		shared_libr = yes
	endif
endif

# If we don't have a shared library and the platform doesn't allow it
# to work without, we have to skip it.
ifneq (,$(findstring yes, $(shared_libr)$(allow_nonpic_in_shlib)))

override CPPFLAGS := -I$(srcdir) -I$(r_include) $(CPPFLAGS)
override CPPFLAGS += -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -DPKGLIBDIR=\"$(pkglibdir)\" -DDLSUFFIX=\"$(DLSUFFIX)\"
override CPPFLAGS += -DR_HOME_DEFAULT=\"$(r_home_default)\"

REGRESS_OPTS = --dbname=$(PL_TESTDB) --load-language=plr
REGRESS = plr






else # can't build

all:
	@echo ""; \
	 echo "*** Cannot build PL/R because libR is not a shared library." ; \
	 echo "*** You might have to rebuild your R installation.  Refer to"; \
	 echo "*** the documentation for details."; \
	 echo ""

endif # can't build - cannot find libR

else  # can't build - no R_HOME

all:
	@echo ""; \
	 echo "*** Cannot build PL/R because R_HOME cannot be found." ; \
	 echo "*** Refer to the documentation for details."; \
	 echo ""

endif

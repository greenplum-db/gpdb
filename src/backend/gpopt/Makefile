#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for gpopt
#
# IDENTIFICATION
#    $PostgreSQL: pgsql/src/backend/gpopt/Makefile,v 1.15 2008/02/19 10:30:07 petere Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/gpopt
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

BLD_TOP = $(top_builddir)/gpAux
include $(BLD_TOP)/releng/releng.mk

include $(top_builddir)/src/backend/gpopt/gpopt.mk

SUBDIRS = config translate relcache utils

OBJS = CGPOptimizer.o gpdbwrappers.o

include $(top_srcdir)/src/backend/common.mk

ifeq (Darwin, $(UNAME))
	LDLIBFLAGS = -dynamiclib -flat_namespace -undefined dynamic_lookup -Wl
endif

ifeq (Linux, $(UNAME))
	LDLIBFLAGS = -shared -Wl,--version-script=library.ver
endif

ifeq (SunOS, $(UNAME))
	LDLIBFLAGS = -shared -ldl -lrt
endif


all:
	 $(CXX)  $(ARCH_FLAGS) $(LDLIBFLAGS) -lgpos -L$(XERCES_LIBDIR) -lxerces-c-3.1 -lnaucrates -lgpdbcost -lgpopt -lpthread  -o libdxltranslators.$(LDSFX) $(shell find . -name '*.o' ! -name 'SUBSYS.o')

install:
	cp $(BLD_TOP)/../src/backend/gpopt/libdxltranslators.$(LDSFX) $(INSTLOC);

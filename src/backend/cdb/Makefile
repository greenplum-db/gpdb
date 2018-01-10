#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for cdb (GPDB specific routines)
#
#-------------------------------------------------------------------------


subdir = src/backend/cdb
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global
override CPPFLAGS := -I$(libpq_srcdir) $(CPPFLAGS)

SUBDIRS := motion dispatcher


OBJS = cdbappendonlystorage.o cdbappendonlystorageformat.o \
       cdbappendonlystorageread.o cdbappendonlystoragewrite.o \
	   cdbbufferedappend.o cdbbufferedread.o \
	   cdbcat.o cdbcopy.o \
	   cdbdirectopen.o \
	   cdbdistributedsnapshot.o \
	   cdbdistributedxid.o cdbdistributedxacts.o \
	   cdbdoublylinked.o \
	   cdbdtxcontextinfo.o \
	   cdbexplain.o \
	   cdbfts.o \
	   cdbgroup.o \
	   cdbhash.o cdbheap.o \
	   cdbllize.o cdblocaldistribxact.o \
	   cdbappendonlyxlog.o \
	   cdbmutate.o \
	   cdboidsync.o \
	   cdbpartindex.o \
	   cdbpartition.o \
	   cdbpath.o cdbpathlocus.o cdbpathtoplan.o \
	   cdbpgdatabase.o \
	   cdbplan.o cdbpullup.o \
	   cdbrelsize.o \
	   cdbshareddoublylinked.o cdbsharedoidsearch.o \
	   cdbsetop.o cdbsreh.o cdbsrlz.o cdbsubplan.o cdbsubselect.o \
	   cdbtargeteddispatch.o cdbthreadlog.o \
	   cdbtimer.o \
	   cdbtm.o cdbtmutils.o \
	   cdbutil.o \
	   cdbvars.o cdbvarblock.o \
	   partitionselection.o

ifeq ($(PORTNAME),cygwin)
.LIBPATTERNS := $(filter-out %.so,$(.LIBPATTERNS))
endif



ALLOBJS = $(OBJS) $(SUBDIROBJS)

include $(top_srcdir)/src/backend/common.mk

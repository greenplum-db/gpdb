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
	   cdbbackup.o cdbbufferedappend.o cdbbufferedread.o \
	   cdbcat.o cdbcopy.o \
	   cdbdatabaseinfo.o cdbdirectopen.o \
	   cdbdistributedsnapshot.o \
	   cdbdistributedxid.o cdbdistributedxacts.o \
	   cdbdoublylinked.o \
	   cdbdtxcontextinfo.o \
	   cdbexplain.o \
	   cdbfilerep.o cdbfilerepservice.o cdbfilerepprimaryrecovery.o \
	   cdbfilerepprimary.o cdbfilerepmirror.o \
	   cdbfilerepprimaryack.o cdbfilerepmirrorack.o \
	   cdbfilerepconnclient.o cdbfilerepconnserver.o \
	   cdbfilerepresyncmanager.o cdbfilerepresyncworker.o \
	   cdbfilerepresetpeerprocess.o \
	   cdbfts.o \
	   cdbglobalsequence.o \
	   cdbgroup.o \
	   cdbhash.o cdbheap.o \
	   cdbllize.o cdblocaldistribxact.o \
	   cdbmirroredbufferpool.o \
	   cdbmirroredfilesysobj.o cdbmirroredflatfile.o \
	   cdbmirroredappendonly.o \
	   cdbmutate.o \
	   cdboidsync.o \
	   cdbpartindex.o \
	   cdbpartition.o \
	   cdbpath.o cdbpathlocus.o cdbpathtoplan.o \
	   cdbpersistentbuild.o cdbpersistentfilespace.o \
	   cdbpersistentrelation.o cdbpersistentdatabase.o cdbpersistenttablespace.o \
	   cdbpersistentfilesysobj.o cdbpersistentrecovery.o cdbpersistentstore.o \
	   cdbpgdatabase.o \
	   cdbplan.o cdbpullup.o \
	   cdbrelsize.o cdbresynchronizechangetracking.o \
	   cdbshareddoublylinked.o cdbsharedoidsearch.o \
	   cdbsetop.o cdbsreh.o cdbsrlz.o cdbsubplan.o cdbsubselect.o \
	   cdbtargeteddispatch.o cdbthreadlog.o \
	   cdbtimer.o \
	   cdbtm.o cdbtmutils.o \
	   cdbutil.o \
	   cdbvars.o cdbvarblock.o \
	   cdbpersistentcheck.o \
	   partitionselection.o

ifeq ($(PORTNAME),cygwin)
.LIBPATTERNS := $(filter-out %.so,$(.LIBPATTERNS))
endif



ALLOBJS = $(OBJS) $(SUBDIROBJS)

include $(top_srcdir)/src/backend/common.mk

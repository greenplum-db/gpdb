#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for executor
#
# IDENTIFICATION
#    $PostgreSQL: pgsql/src/backend/executor/Makefile,v 1.27 2008/02/19 10:30:07 petere Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/executor
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global
override CPPFLAGS := -I$(libpq_srcdir) $(CPPFLAGS)


OBJS = execAmi.o execCurrent.o execGrouping.o execJunk.o execMain.o \
       execProcnode.o execQual.o execScan.o execTuples.o \
       execUtils.o functions.o instrument.o nodeAppend.o nodeAgg.o \
       nodeBitmapAnd.o nodeBitmapOr.o \
       nodeBitmapHeapscan.o nodeBitmapIndexscan.o nodeHash.o \
       nodeHashjoin.o nodeIndexscan.o nodeMaterial.o nodeMergejoin.o \
       nodeNestloop.o nodeFunctionscan.o nodeRecursiveunion.o nodeResult.o \
       nodeSetOp.o nodeSort.o nodeUnique.o \
       nodeValuesscan.o nodeCtescan.o nodeWorktablescan.o \
       nodeLimit.o nodeSubplan.o nodeSubqueryscan.o nodeTidscan.o \
       tstoreReceiver.o spi.o \
       nodeBitmapTableScan.o \
       nodeExternalscan.o \
       nodeDynamicIndexscan.o \
       nodeDynamicBitmapIndexscan.o \
       nodeMotion.o \
       nodeRepeat.o \
       nodeShareInputScan.o \
       nodeTableFunction.o \
       nodeTableScan.o nodeDynamicTableScan.o \
       nodeWindow.o \
       nodeSequence.o \
       nodeRowTrigger.o \
       nodeAssertOp.o \
       nodeSplitUpdate.o \
       nodeDML.o \
       execDML.o \
       nodePartitionSelector.o \
       execBitmapTableScan.o execBitmapHeapScan.o execBitmapAOScan.o \
       execDynamicScan.o \
       execHHashagg.o execGpmon.o execWorkfile.o execHeapScan.o execAOScan.o \
       execAOCSScan.o nodeBitmapAppendOnlyscan.o

include $(top_srcdir)/src/backend/common.mk

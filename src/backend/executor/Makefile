#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for executor
#
# IDENTIFICATION
#    $PostgreSQL: pgsql/src/backend/executor/Makefile,v 1.29 2008/12/28 18:53:55 tgl Exp $
#
#-------------------------------------------------------------------------

subdir = src/backend/executor
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global
override CPPFLAGS := -I$(top_srcdir)/src/backend/gp_libpq_fe $(CPPFLAGS)


OBJS = execAmi.o execCurrent.o execGrouping.o execJunk.o execMain.o \
       execProcnode.o execQual.o execScan.o execTuples.o \
       execUtils.o functions.o instrument.o nodeAppend.o nodeAgg.o \
       nodeBitmapAnd.o nodeBitmapOr.o \
       nodeBitmapHeapscan.o nodeBitmapIndexscan.o nodeHash.o \
       nodeHashjoin.o nodeIndexscan.o nodeMaterial.o nodeMergejoin.o \
       nodeNestloop.o nodeFunctionscan.o nodeResult.o \
       nodeSetOp.o nodeSort.o nodeUnique.o \
       nodeValuesscan.o nodeLimit.o \
       nodeSubplan.o nodeSubqueryscan.o nodeTidscan.o tstoreReceiver.o spi.o \
       nodeBitmapTableScan.o \
       nodeExternalscan.o \
       nodeDynamicIndexscan.o \
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
       execDynamicScan.o execDynamicIndexScan.o \
       execIndexscan.o \
       execHHashagg.o execGpmon.o execWorkfile.o execHeapScan.o execAOScan.o \
       execAOCSScan.o nodeBitmapAppendOnlyscan.o
include $(top_srcdir)/src/backend/common.mk

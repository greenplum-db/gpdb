#
# Makefile for optimizer
#
# src/backend/gporca/libnaucrates/src/statistics/Makefile
#

subdir = src/backend/gporca/libnaucrates/src/statistics
top_builddir = ../../../../../..
include $(top_builddir)/src/Makefile.global

include $(top_srcdir)/src/backend/gporca/gporca.mk

OBJS        = CBucket.o \
              CFilterStatsProcessor.o \
              CGroupByStatsProcessor.o \
              CHistogram.o \
              CInnerJoinStatsProcessor.o \
              CJoinStatsProcessor.o \
              CLeftAntiSemiJoinStatsProcessor.o \
              CLeftOuterJoinStatsProcessor.o \
              CLeftSemiJoinStatsProcessor.o \
              CLimitStatsProcessor.o \
              CPoint.o \
              CProjectStatsProcessor.o \
              CScaleFactorUtils.o \
              CStatistics.o \
              CStatisticsUtils.o \
              CStatsPredConj.o \
              CStatsPredDisj.o \
              CStatsPredLike.o \
              CStatsPredPoint.o \
              CStatsPredArrayCmp.o \
              CStatsPredUnsupported.o \
              CStatsPredUtils.o \
              CUnionAllStatsProcessor.o \
              CUpperBoundNDVs.o

include $(top_srcdir)/src/backend/common.mk


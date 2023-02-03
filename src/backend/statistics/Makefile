#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for statistics
#
# IDENTIFICATION
#    src/backend/statistics/Makefile
#
#-------------------------------------------------------------------------

subdir = src/backend/statistics
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

OBJS = extended_stats.o dependencies.o mcv.o mvdistinct.o statistics_gram.o

statistics_gram.o: statistics_scanner.c

include $(top_srcdir)/src/backend/common.mk

# statistics_gram.c, statistics_scanner.c are not cleaned here.
# (Our parent Makefile takes care of them during maintainer-clean.)

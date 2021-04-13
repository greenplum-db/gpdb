#Intel compiler
ifeq "$(CC)" "icc"
	USE_ICC = 1
endif

override CFLAGS+=-std=gnu99

MODULE_big = gp_svec
EXTENSION = gp_sparse_vector
DATA = gp_sparse_vector--1.0.1.sql gp_sparse_vector--1.0.0--1.0.1.sql
REGRESS = gp_svec gp_svec_features

OBJS = gp_sfv.o sparse_vector.o operators.o SparseData.o

ifdef USE_PGXS
	PGXS := $(shell pg_config --pgxs)
	include $(PGXS)
else
	subdir = gpcontrib/gp_sparse_vector
	top_builddir = ../..
	include $(top_builddir)/src/Makefile.global
	include $(top_srcdir)/contrib/contrib-global.mk
endif

ifdef USE_ICC
	override CFLAGS=-O3 -Werror -std=c99 -vec-report2 -vec-threshold0
endif

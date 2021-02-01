EXTENSION = extended_protocol_commit_test_fdw
DATA = extended_protocol_commit_test_fdw--1.0.sql

MODULE_big = extended_protocol_commit_test_fdw

OBJS = extended_protocol_commit_test_fdw.o


subdir = src/test/fdw/extension
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global
NO_PGXS = 1
include $(top_srcdir)/src/makefiles/pgxs.mk


installcheck: install

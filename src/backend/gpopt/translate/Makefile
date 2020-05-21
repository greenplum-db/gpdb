#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for gpopt
#
#-------------------------------------------------------------------------

subdir = src/backend/gpopt/translate
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

include $(top_srcdir)/src/backend/gpopt/gpopt.mk

OBJS =  CMappingColIdVar.o \
		CMappingVarColId.o \
		CMappingElementColIdParamId.o \
		CMappingColIdVarPlStmt.o \
		CDXLTranslateContextBaseTable.o \
		CCTEListEntry.o \
		CContextDXLToPlStmt.o \
		CDXLTranslateContext.o \
		CQueryMutators.o \
		CTranslatorScalarToDXL.o \
		CTranslatorDXLToScalar.o \
		CTranslatorUtils.o \
		CTranslatorRelcacheToDXL.o \
		CContextQueryToDXL.o \
		CTranslatorQueryToDXL.o \
		CTranslatorDXLToPlStmt.o 

include $(top_srcdir)/src/backend/common.mk

#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for gpopt
#
#-------------------------------------------------------------------------

subdir = src/backend/gpopt/translate
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

OBJS =  CMappingColIdVar.o \
		CMappingVarColId.o \
		CMappingElementColIdParamId.o \
		CMappingColIdVarPlStmt.o \
		CMappingElementColIdTE.o \
		CDXLTranslateContextBaseTable.o \
		CCTEListEntry.o \
		CContextDXLToPlStmt.o \
		CDXLTranslateContext.o \
		CQueryMutators.o \
		CTranslatorScalarToDXL.o \
		CTranslatorDXLToScalar.o \
		CTranslatorUtils.o \
		CTranslatorRelcacheToDXL.o \
		CTranslatorQueryToDXL.o \
		CTranslatorDXLToPlStmt.o 

include $(top_srcdir)/src/backend/common.mk

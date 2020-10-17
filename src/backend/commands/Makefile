#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for backend/commands
#
# IDENTIFICATION
#    src/backend/commands/Makefile
#
#-------------------------------------------------------------------------

subdir = src/backend/commands
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global
override CPPFLAGS := -I$(libpq_srcdir) $(CPPFLAGS)

explain.o: explain_gp.c

OBJS = amcmds.o aggregatecmds.o alter.o analyze.o async.o cluster.o comment.o \
	collationcmds.o constraint.o conversioncmds.o copy.o createas.o \
	dbcommands.o define.o discard.o dropcmds.o \
	event_trigger.o explain.o extension.o foreigncmds.o functioncmds.o \
	indexcmds.o lockcmds.o matview.o operatorcmds.o opclasscmds.o \
	policy.o portalcmds.o prepare.o proclang.o publicationcmds.o \
	schemacmds.o seclabel.o sequence.o statscmds.o subscriptioncmds.o \
	tablecmds.o tablespace.o trigger.o tsearchcmds.o typecmds.o user.o \
	vacuum.o variable.o view.o

OBJS += analyzefuncs.o analyzeutils.o extprotocolcmds.o exttablecmds.o queue.o
OBJS += resgroupcmds.o tablecmds_gp.o vacuum_ao.o

include $(top_srcdir)/src/backend/common.mk

/*-------------------------------------------------------------------------
 *
 * autostats.h
 *	  header file for Greenplum auto-analyze functions
 *
 *
 * Portions Copyright (c) 2005-2015, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/postmaster/autostats.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef AUTOSTATS_H
#define AUTOSTATS_H

#include "executor/execdesc.h"

/**
 * AutoStatsCmdType - an enumeration of type of statements known to auto-stats
 * module. 
 * If adding a new statement type, ensure that SENTINEL is the last entry and
 * autostats_cmdtype_to_string knows of the new type. 
 */
typedef enum AutoStatsCmdType
{
	AUTOSTATS_CMDTYPE_CTAS,
	AUTOSTATS_CMDTYPE_UPDATE,
	AUTOSTATS_CMDTYPE_INSERT,
	AUTOSTATS_CMDTYPE_DELETE,
	AUTOSTATS_CMDTYPE_COPY,

	/*
	 * This should always be the last entry. add new statement types before
	 * this entry.
	 */
	AUTOSTATS_CMDTYPE_SENTINEL
} AutoStatsCmdType;

extern const char *autostats_cmdtype_to_string(AutoStatsCmdType cmdType);
extern void autostats_get_cmdtype(QueryDesc *queryDesc,
					  AutoStatsCmdType *pcmdType, Oid *prelationOid);
extern void auto_stats(AutoStatsCmdType cmdType, Oid relationOid,
		   uint64 ntuples, bool inFunction);

#endif   /* AUTOSTATS_H */

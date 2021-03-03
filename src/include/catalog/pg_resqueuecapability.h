/*-------------------------------------------------------------------------
 *
 * pg_resqueuecapability.h
 *	  definition of the system "pg_resqueuecapability" relation.
 *
 *
 * Portions Copyright (c) 2006-2010, Greenplum inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * NOTES
 *	  the genbki.sh script reads this file and generates .bki
 *	  information from the DATA() statements.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_RESQUEUECAPABILITY_H
#define PG_RESQUEUECAPABILITY_H

#include "catalog/genbki.h"
#include "catalog/pg_resqueuecapability_d.h"

/* ----------------
 *		pg_resqueuecapability definition.  cpp turns this into
 *		typedef struct FormData_pg_resqueuecapability
 * ----------------
 */

CATALOG(pg_resqueuecapability,6060,ResQueueCapabilityRelationId) BKI_SHARED_RELATION
{
	Oid		resqueueid;	/* OID of the queue with this capability  */
	int16	restypid;	/* resource type id (key to pg_resourcetype)  */
#ifdef CATALOG_VARLEN			/* variable-length fields start here */
	text	ressetting;	/* resource setting (opaque type)  */
#endif
} FormData_pg_resqueuecapability;

/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(resqueueid REFERENCES pg_resqueue(oid));
FOREIGN_KEY(restypid REFERENCES pg_resourcetype(restypid));

/* ----------------
 *		Form_pg_resqueuecapability corresponds to a pointer to a tuple with
 *		the format of pg_resqueuecapability relation.
 * ----------------
 */
typedef FormData_pg_resqueuecapability *Form_pg_resqueuecapability;

#endif   /* PG_RESQUEUECAPABILITY_H */

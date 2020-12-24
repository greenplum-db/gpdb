/*-------------------------------------------------------------------------
 *
 * pg_resqueue.h
 *	  definition of the system "resource queue" relation (pg_resqueue).
 *
 *
 * Portions Copyright (c) 2006-2010, Greenplum inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/pg_resqueue.h
 *
 * NOTES
 *	  the genbki.sh script reads this file and generates .bki
 *	  information from the DATA() statements.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_RESQUEUE_H
#define PG_RESQUEUE_H


#include "catalog/genbki.h"
#include "catalog/pg_resqueue_d.h"
#include "parser/parsetree.h"
#include "tcop/pquery.h"

/* ----------------
 *		pg_resqueue definition.  cpp turns this into
 *		typedef struct FormData_pg_resqueue
 * ----------------
 */

CATALOG(pg_resqueue,6026,ResQueueRelationId) BKI_SHARED_RELATION
{
	Oid			oid;				/* oid */
	NameData	rsqname;			/* name of resource queue */
	float4		rsqcountlimit;		/* max active count limit */
	float4		rsqcostlimit;		/* max cost limit */
	bool		rsqovercommit;		/* allow overcommit on suitable  limits */
	float4		rsqignorecostlimit;	/* ignore queries with cost less than */
} FormData_pg_resqueue;

/* no foreign keys */

/* ----------------
 *		Form_pg_resqueue corresponds to a pointer to a tuple with
 *		the format of pg_resqueue relation.
 * ----------------
 */
typedef FormData_pg_resqueue *Form_pg_resqueue;

#endif   /* PG_RESQUEUE_H */

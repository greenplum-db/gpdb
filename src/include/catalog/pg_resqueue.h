/*-------------------------------------------------------------------------
 *
 * pg_resqueue.h
 *	  definition of the system "resource queue" relation (pg_resqueue).
 *
 *
 * Portions Copyright (c) 2006-2010, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
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
#include "parser/parsetree.h"
#include "tcop/pquery.h"

/* ----------------
 *		pg_resqueue definition.  cpp turns this into
 *		typedef struct FormData_pg_resqueue
 * ----------------
 */
#define ResQueueRelationId	6026

CATALOG(pg_resqueue,6026) BKI_SHARED_RELATION
{
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


/* ----------------
 *		compiler constants for pg_resqueue
 * ----------------
 */
#define Natts_pg_resqueue					5
#define Anum_pg_resqueue_rsqname			1
#define Anum_pg_resqueue_rsqcountlimit		2
#define Anum_pg_resqueue_rsqcostlimit		3
#define Anum_pg_resqueue_rsqovercommit		4
#define Anum_pg_resqueue_rsqignorecostlimit	5

/* Create initial default resource queue */
DATA(insert OID = 6055 ( pg_default 20 -1 f 0 ));

#define DEFAULTRESQUEUE_OID 6055

#endif   /* PG_RESQUEUE_H */

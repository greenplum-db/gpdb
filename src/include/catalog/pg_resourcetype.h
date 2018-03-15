/*-------------------------------------------------------------------------
 *
 * pg_resourcetype.h
 *	  definition of the system "pg_resourcetype" relation.
 *
 *
 * Portions Copyright (c) 2006-2010, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
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
#ifndef PG_RESOURCETYPE_H
#define PG_RESOURCETYPE_H


#include "catalog/genbki.h"

/*
  The flavors of resource types:

  required: user must specify this type during CREATE.  Every queue
  must always have this type entry in pg_resqueuecapability.  It is
  not required to have a default value.  It may or may not have an
  offvalue, depending on the "has disable" setting.

  optional (ie not required): user does not have to specify this type.
  If hasdefault is false, then no entry is required for
  pg_resqueuecapability.  If hasdefault is true, then CREATE will add
  the default entry to pg_resqueuecapability during CREATE.

  has disable: whether the resource type has an OFF switch, ie what is
  the WITHOUT behavior.  For a required type, if it can be disabled,
  then it must have an off value.  For an optional type, if it can be
  disabled, there are two options:

  1) if the optional type has a default value, then if must have an
     off value.  The off value can be the same as the default value.

  2) if the optional type does not have a default value, then the
     assumption is that it gets "shut off" by removing the
     pg_resqueuecapability entry.  The off value is ignored.  Which
     means if hasdefault is false, and required is false, then
     hasdisable must be true (because the CREATE statement won't add
     an entry for the type, so it is de facto disabled).

 */

/* MPP-6923: Resource Queue attribute flexibility */

/* ----------------
 *		pg_resourcetype definition.  cpp turns this into
 *		typedef struct FormData_pg_resourcetype
 * ----------------
 */
#define ResourceTypeRelationId	6059

CATALOG(pg_resourcetype,6059) BKI_SHARED_RELATION
{
	NameData	resname;			/* name of resource type  */
	int2		restypid;			/* resource type id  */
	bool		resrequired;		/* if required, user must specify during CREATE */
	bool		reshasdefault;		/* create a default entry for optional type */
	bool		reshasdisable;		/* whether the type can be removed or shut off */
	text		resdefaultsetting;	/* default resource setting  */
	text		resdisabledsetting;	/* value that turns it off  */
} FormData_pg_resourcetype;

/* no foreign keys */

/* ----------------
 *		Form_pg_resourcetype corresponds to a pointer to a tuple with
 *		the format of pg_resourcetype relation.
 * ----------------
 */
typedef FormData_pg_resourcetype *Form_pg_resourcetype;


/* ----------------
 *		compiler constants for pg_resourcetype
 * ----------------
 */
#define Natts_pg_resourcetype					7
#define Anum_pg_resourcetype_resname			1
#define Anum_pg_resourcetype_restypid			2
#define Anum_pg_resourcetype_resrequired		3
#define Anum_pg_resourcetype_reshasdefault		4
#define Anum_pg_resourcetype_reshasdisable		5
#define Anum_pg_resourcetype_resdefaultsetting	6
#define Anum_pg_resourcetype_resdisabledsetting	7

/* Create entry in pg_resourcetype for PRIORITY */
DATA(insert OID = 6454  ( active_statements 1 f t t -1  -1 ));
DATA(insert OID = 6455  ( max_cost 2 f t t -1  -1 ));
DATA(insert OID = 6456  ( min_cost 3 f t t -1   0 ));
DATA(insert OID = 6457  ( cost_overcommit 4 f t t -1  -1 )); 
DATA(insert OID = 6458  ( priority 5 f t f medium _null_ ));
DATA(insert OID = 6459  ( memory_limit 6 f t t -1 -1 ));

/* 
   The first four entries of pg_resourcetype are special mappings for
   the original pg_resqueue columns.  The following table shows the
   correspondence between the original grammar, the pg_resqueue column
   name, and the WITH clause defnames.

  grammar           colname             orig_defname        new_defname 
 ------------------ -----------------   ---------------     -----------------
 "ACTIVE THRESHOLD" rsqcountlimit       activelimit         active_statements
 "COST THRESHOLD"   rsqcostlimit        costlimit           max_cost
 "IGNORE THRESHOLD" rsqignorecostlimit  ignorecostlimit     min_cost
 "OVERCOMMIT"       rsqovercommit       overcommit          cost_overcommit
*/

/* Note: the restypid is used by pg_dumpall.c to build CREATE statements */

#define PG_RESRCTYPE_ACTIVE_STATEMENTS	1	/* rsqcountlimit: count  */
#define PG_RESRCTYPE_MAX_COST			2	/* rsqcostlimit: max_cost */
#define PG_RESRCTYPE_MIN_COST			3	/* rsqignorecostlimit: min_cost */
#define PG_RESRCTYPE_COST_OVERCOMMIT	4	/* rsqovercommit: cost_overcommit*/
/* start of "pg_resourcetype" entries... */
#define PG_RESRCTYPE_PRIORITY			5	/* backoff.c: priority queue */
#define PG_RESRCTYPE_MEMORY_LIMIT		6	/* memquota.c: memory quota */

#endif   /* PG_RESOURCETYPE_H */

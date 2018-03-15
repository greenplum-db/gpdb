/*-------------------------------------------------------------------------
 *
 * pg_stat_last_shoperation.h
 *
 *
 * Portions Copyright (c) 2006-2010, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2010, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * NOTES
 *	  the genbki.pl script reads this file and generates .bki
 *	  information from the DATA() statements.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_STAT_LAST_SHOPERATION_H
#define PG_STAT_LAST_SHOPERATION_H

#include "catalog/genbki.h"

/* here is the "shared" version */

#define timestamptz Datum

#define StatLastShOpRelationName		"pg_stat_last_shoperation"

#define StatLastShOpRelationId 6056

CATALOG(pg_stat_last_shoperation,6056)  BKI_SHARED_RELATION BKI_WITHOUT_OIDS
{
	/* unique key */
	Oid			classid;		/* OID of table containing object */
	Oid			objid;			/* OID of object itself */
	NameData	staactionname;	/* name of action */

	/* */
	Oid			stasysid;		/* OID of user (when action was performed) */
	NameData	stausename;		/* name of user (when action was performed) */
	text		stasubtype;		/* action subtype */
	timestamptz	statime;
} FormData_pg_statlastshop;

/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(classid REFERENCES pg_class(oid));
FOREIGN_KEY(stasysid REFERENCES pg_authid(oid));

#undef timestamptz

/* ----------------
 *		Form_pg_statlastshop corresponds to a pointer to a tuple with
 *		the format of pg_statlastshop relation.
 * ----------------
 */
typedef FormData_pg_statlastshop *Form_pg_statlastshop;

/* ----------------
 *		compiler constants for pg_stat_last_shoperation
 * ----------------
 */
#define Natts_pg_statlastshop				7
#define Anum_pg_statlastshop_classid		1
#define Anum_pg_statlastshop_objid			2
#define Anum_pg_statlastshop_staactionname	3
#define Anum_pg_statlastshop_stasysid		4
#define Anum_pg_statlastshop_stausename		5
#define Anum_pg_statlastshop_stasubtype		6
#define Anum_pg_statlastshop_statime		7

#endif   /* PG_STAT_LAST_SHOPERATION_H */

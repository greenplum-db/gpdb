/*-------------------------------------------------------------------------
 *
 * gp_stat_last_shoperation.h
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
#ifndef GP_STAT_LAST_SHOPERATION_H
#define GP_STAT_LAST_SHOPERATION_H

#include "catalog/genbki.h"

/* here is the "shared" version */

#define timestamptz Datum

#define StatLastShOpRelationName		"gp_stat_last_shoperation"

#define StatLastShOpRelationId 6056

CATALOG(gp_stat_last_shoperation,6056)  BKI_SHARED_RELATION BKI_WITHOUT_OIDS
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
} FormData_gp_statlastshop;

/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(classid REFERENCES pg_class(oid));
FOREIGN_KEY(stasysid REFERENCES pg_authid(oid));

#undef timestamptz

/* ----------------
 *		Form_gp_statlastshop corresponds to a pointer to a tuple with
 *		the format of gp_stat_last_shoperation relation.
 * ----------------
 */
typedef FormData_gp_statlastshop *Form_gp_statlastshop;

/* ----------------
 *		compiler constants for gp_stat_last_shoperation
 * ----------------
 */
#define Natts_gp_statlastshop				7
#define Anum_gp_statlastshop_classid		1
#define Anum_gp_statlastshop_objid			2
#define Anum_gp_statlastshop_staactionname	3
#define Anum_gp_statlastshop_stasysid		4
#define Anum_gp_statlastshop_stausename		5
#define Anum_gp_statlastshop_stasubtype		6
#define Anum_gp_statlastshop_statime		7

#endif   /* GP_STAT_LAST_SHOPERATION_H */

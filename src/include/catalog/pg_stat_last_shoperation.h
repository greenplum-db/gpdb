/*-------------------------------------------------------------------------
 *
 * pg_stat_last_shoperation.h
 *
 *
 * Portions Copyright (c) 2006-2010, Greenplum inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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
#include "catalog/pg_stat_last_shoperation_d.h"

/* here is the "shared" version */

#define timestamptz Datum

CATALOG(pg_stat_last_shoperation,6056,StatLastShOpRelationId)  BKI_SHARED_RELATION
{
	/* unique key */
	Oid			classid;		/* OID of table containing object */
	Oid			objid;			/* OID of object itself */
	NameData	staactionname;	/* name of action */

	/* */
	Oid			stasysid;		/* OID of user (when action was performed) */
	NameData	stausename;		/* name of user (when action was performed) */
#ifdef CATALOG_VARLEN
	text		stasubtype;		/* action subtype */
	timestamptz	statime;
#endif
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

#endif   /* PG_STAT_LAST_SHOPERATION_H */

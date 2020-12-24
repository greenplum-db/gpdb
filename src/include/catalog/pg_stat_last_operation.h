/*-------------------------------------------------------------------------
 *
 * pg_stat_last_operation.h
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
#ifndef PG_STAT_LAST_OPERATION_H
#define PG_STAT_LAST_OPERATION_H

#include "catalog/genbki.h"
#include "catalog/pg_stat_last_operation_d.h"

/*
 * The CATALOG definition has to refer to the type of log_time as
 * "timestamptz" (lower case) so that bootstrap mode recognizes it.  But
 * the C header files define this type as TimestampTz.	Since the field is
 * potentially-null and therefore can't be accessed directly from C code,
 * there is no particular need for the C struct definition to show the
 * field type as TimestampTz --- instead we just make it Datum.
 */

#define timestamptz Datum

CATALOG(pg_stat_last_operation,6052,StatLastOpRelationId)
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
} FormData_pg_statlastop;


/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(classid REFERENCES pg_class(oid));
FOREIGN_KEY(stasysid REFERENCES pg_authid(oid));

#undef timestamptz

/* ----------------
 *		Form_pg_statlastop corresponds to a pointer to a tuple with
 *		the format of pg_statlastop relation.
 * ----------------
 */
typedef FormData_pg_statlastop *Form_pg_statlastop;

#endif   /* PG_STAT_LAST_OPERATION_H */

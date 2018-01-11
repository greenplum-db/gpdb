/*-------------------------------------------------------------------------
 *
 * pg_tablespace.h
 *	  definition of the system "tablespace" relation (pg_tablespace)
 *	  along with the relation's initial contents.
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/catalog/pg_tablespace.h,v 1.12 2009/01/01 17:23:58 momjian Exp $
 *
 * NOTES
 *	  the genbki.sh script reads this file and generates .bki
 *	  information from the DATA() statements.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_TABLESPACE_H
#define PG_TABLESPACE_H

#include "catalog/genbki.h"

/* ----------------
 *		pg_tablespace definition.  cpp turns this into
 *		typedef struct FormData_pg_tablespace
 * ----------------
 */
#define TableSpaceRelationId  1213

CATALOG(pg_tablespace,1213) BKI_SHARED_RELATION
{
	NameData	spcname;		/* tablespace name */
	Oid			spcowner;		/* owner of tablespace */
	text		spclocation;	/* physical location (VAR LENGTH) */
	aclitem		spcacl[1];		/* access permissions (VAR LENGTH) */
	Oid			spcfsoid;		/* FilespaceOid */
} FormData_pg_tablespace;

/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(spcowner REFERENCES pg_authid(oid));
FOREIGN_KEY(spcfsoid REFERENCES pg_filespace(oid));

/* ----------------
 *		Form_pg_tablespace corresponds to a pointer to a tuple with
 *		the format of pg_tablespace relation.
 * ----------------
 */
typedef FormData_pg_tablespace *Form_pg_tablespace;

/* ----------------
 *		compiler constants for pg_tablespace
 * ----------------
 */

#define Natts_pg_tablespace				5
#define Anum_pg_tablespace_spcname		1
#define Anum_pg_tablespace_spcowner		2
#define Anum_pg_tablespace_spclocation  3
#define Anum_pg_tablespace_spcacl       4
#define Anum_pg_tablespace_spcfsoid	    5

DATA(insert OID = 1663 ( pg_default PGUID _null_ _null_ 3052 ));
DATA(insert OID = 1664 ( pg_global	PGUID _null_ _null_ 3052 ));

#define DEFAULTTABLESPACE_OID 1663
#define GLOBALTABLESPACE_OID 1664

#define IsBuiltinTablespace(tablespaceId) \
	(tablespaceId == DEFAULTTABLESPACE_OID || \
	 tablespaceId == GLOBALTABLESPACE_OID)

#endif   /* PG_TABLESPACE_H */

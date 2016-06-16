/*-------------------------------------------------------------------------
 *
 * pg_tablespace.h
 *	  definition of the system "tablespace" relation (pg_tablespace)
 *	  along with the relation's initial contents.
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/catalog/pg_tablespace.h,v 1.10 2008/01/01 19:45:57 momjian Exp $
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

/*
 * pg_tablespace
 *
 * Note: spclocation, spcprilocations and spcmirlocations are all
 * deprecated and will always be null.  They are left in place only
 * due to upgrade concerns.
 *
 * If an upgrade strategy is developed that will allow dropping of
 * columns then this table should be restructured.
 */
/* TIDYCAT_BEGINFAKEDEF

   CREATE TABLE pg_tablespace
   with (camelcase=TableSpace, shared=true, relid=1213)
   (
   spcname          name, 
   spcowner         oid, 
   spclocation      text, 
   spcacl           aclitem[], 
   spcprilocations  text[], 
   spcmirlocations  text[], 
   spcfsoid         oid
   );

   create unique index on pg_tablespace(oid) with (indexid=2697, CamelCase=TablespaceOid);
   create unique index on pg_tablespace(spcname) with (indexid=2698, CamelCase=TablespaceName);

   alter table pg_tablespace add fk spcowner on pg_authid(oid);
   alter table pg_tablespace add fk spcfsoid on pg_filespace(oid);

   TIDYCAT_ENDFAKEDEF
*/


/* ----------------
 *		pg_tablespace definition.  cpp turns this into
 *		typedef struct FormData_pg_tablespace
 * ----------------
 */
#define TableSpaceRelationId  1213

/*
 * pg_tablespace
 *
 * Note: spclocation, spcprilocations and spcmirlocations are all deprecated
 * and will always be null.  They are left in place only due to upgrade concerns.
 * If an upgrade strategy is developed that will allow dropping of columns then
 * this table should be restructured.
 */
CATALOG(pg_tablespace,1213) BKI_SHARED_RELATION
{
	NameData	spcname;		    /* tablespace name */
	Oid			spcowner;		    /* owner of tablespace */
	text		spclocation;	    /* physical location (VAR LENGTH) */
	aclitem		spcacl[1];          /* access permissions (VAR LENGTH) */
	text		spcprilocations[1];	/* primary segment physical location */
	text		spcmirlocations[1];	/* mirror segment physical location  */
	Oid         spcfsoid;           /* FilespaceOid */
} FormData_pg_tablespace;

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

#define Natts_pg_tablespace				7
#define Anum_pg_tablespace_spcname		1
#define Anum_pg_tablespace_spcowner		2
#define Anum_pg_tablespace_deprecated_1 3
#define Anum_pg_tablespace_spcacl       4
#define Anum_pg_tablespace_deprecated_2	5
#define Anum_pg_tablespace_deprecated_3	6
#define Anum_pg_tablespace_spcfsoid	    7

DATA(insert OID = 1663 ( pg_default PGUID _null_ _null_ _null_ _null_ 3052 ));
DATA(insert OID = 1664 ( pg_global	PGUID _null_ _null_ _null_ _null_ 3052 ));

#define DEFAULTTABLESPACE_OID 1663
#define GLOBALTABLESPACE_OID 1664

#define IsBuiltinTablespace(tablespaceId) \
	(tablespaceId == DEFAULTTABLESPACE_OID || \
	 tablespaceId == GLOBALTABLESPACE_OID)

#endif   /* PG_TABLESPACE_H */

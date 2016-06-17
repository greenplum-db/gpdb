/*-------------------------------------------------------------------------
 *
 * pg_attrdef.h
 *	  definition of the system "attribute defaults" relation (pg_attrdef)
 *	  along with the relation's initial contents.
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/catalog/pg_attrdef.h,v 1.22 2008/01/01 19:45:56 momjian Exp $
 *
 * NOTES
 *	  the genbki.sh script reads this file and generates .bki
 *	  information from the DATA() statements.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_ATTRDEF_H
#define PG_ATTRDEF_H

#include "catalog/genbki.h"

/* TIDYCAT_BEGINFAKEDEF

   CREATE TABLE pg_attrdef
   with (camelcase=AttrDefault, relid=2604, toast_oid=2830, toast_index=2831)
   (
   adrelid  oid, 
   adnum    smallint, 
   adbin    text, 
   adsrc    text
   );

   create unique index on pg_attrdef(adrelid, adnum) with (indexid=2656, CamelCase=AttrDefault);
   create unique index on pg_attrdef(oid) with (indexid=2657, CamelCase=AttrDefaultOid);

   alter table pg_attrdef add fk adrelid on pg_attribute(attrelid);

   TIDYCAT_ENDFAKEDEF
*/

/* ----------------
 *		pg_attrdef definition.	cpp turns this into
 *		typedef struct FormData_pg_attrdef
 * ----------------
 */
#define AttrDefaultRelationId  2604

CATALOG(pg_attrdef,2604)
{
	Oid			adrelid;
	int2		adnum;
	text		adbin;
	text		adsrc;
} FormData_pg_attrdef;

/* ----------------
 *		Form_pg_attrdef corresponds to a pointer to a tuple with
 *		the format of pg_attrdef relation.
 * ----------------
 */
typedef FormData_pg_attrdef *Form_pg_attrdef;

/* ----------------
 *		compiler constants for pg_attrdef
 * ----------------
 */
#define Natts_pg_attrdef				4
#define Anum_pg_attrdef_adrelid			1
#define Anum_pg_attrdef_adnum			2
#define Anum_pg_attrdef_adbin			3
#define Anum_pg_attrdef_adsrc			4

#endif   /* PG_ATTRDEF_H */

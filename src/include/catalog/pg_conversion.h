/*-------------------------------------------------------------------------
 *
 * pg_conversion.h
 *	  definition of the system "conversion" relation (pg_conversion)
 *	  along with the relation's initial contents.
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/catalog/pg_conversion.h,v 1.20 2008/01/01 19:45:56 momjian Exp $
 *
 * NOTES
 *	  the genbki.sh script reads this file and generates .bki
 *	  information from the DATA() statements.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_CONVERSION_H
#define PG_CONVERSION_H

#include "catalog/genbki.h"

/* ----------------------------------------------------------------
 *		pg_conversion definition.
 *
 *		cpp turns this into typedef struct FormData_pg_namespace
 *
 *	conname				name of the conversion
 *	connamespace		name space which the conversion belongs to
 *	conowner			owner of the conversion
 *	conforencoding		FOR encoding id
 *	contoencoding		TO encoding id
 *	conproc				OID of the conversion proc
 *	condefault			TRUE if this is a default conversion
 * ----------------------------------------------------------------
 */

/* TIDYCAT_BEGINFAKEDEF

   CREATE TABLE pg_conversion
   with (relid=2607)
   (
   conname         name, 
   connamespace    oid, 
   conowner        oid, 
   conforencoding  integer, 
   contoencoding   integer, 
   conproc         regproc, 
   condefault      boolean
   );

   create unique index on pg_conversion(connamespace, conforencoding, contoencoding, oid) with (indexid=2668, CamelCase=ConversionDefault, syscacheid=CONDEFAULT, syscache_nbuckets=128);

   create unique index on pg_conversion(conname, connamespace) with (indexid=2669, CamelCase=ConversionNameNsp, syscacheid=CONNAMENSP, syscache_nbuckets=128);

   create unique index on pg_conversion(oid) with (indexid=2670, CamelCase=ConversionOid, syscacheid=CONVOID, syscache_nbuckets=128);

   alter table pg_conversion add fk connamespace on pg_namespace(oid);
   alter table pg_conversion add fk conowner on pg_authid(oid);
   alter table pg_conversion add fk conproc on pg_proc(oid);

   TIDYCAT_ENDFAKEDEF
*/

#define ConversionRelationId  2607

CATALOG(pg_conversion,2607)
{
	NameData	conname;
	Oid			connamespace;
	Oid			conowner;
	int4		conforencoding;
	int4		contoencoding;
	regproc		conproc;
	bool		condefault;
} FormData_pg_conversion;

/* ----------------
 *		Form_pg_conversion corresponds to a pointer to a tuple with
 *		the format of pg_conversion relation.
 * ----------------
 */
typedef FormData_pg_conversion *Form_pg_conversion;

/* ----------------
 *		compiler constants for pg_conversion
 * ----------------
 */

#define Natts_pg_conversion				7
#define Anum_pg_conversion_conname		1
#define Anum_pg_conversion_connamespace 2
#define Anum_pg_conversion_conowner		3
#define Anum_pg_conversion_conforencoding		4
#define Anum_pg_conversion_contoencoding		5
#define Anum_pg_conversion_conproc		6
#define Anum_pg_conversion_condefault	7

/* ----------------
 * initial contents of pg_conversion
 * ---------------
 */

/*
 * prototypes for functions in pg_conversion.c
 */
#include "nodes/parsenodes.h"

extern Oid ConversionCreate(const char *conname, Oid connamespace,
				 Oid conowner,
				 int32 conforencoding, int32 contoencoding,
				 Oid conproc, bool def, Oid newOid);
extern void ConversionDrop(Oid conversionOid, DropBehavior behavior);
extern void RemoveConversionById(Oid conversionOid);
extern Oid	FindConversion(const char *conname, Oid connamespace);
extern Oid	FindDefaultConversion(Oid connamespace, int32 for_encoding, int32 to_encoding);

#endif   /* PG_CONVERSION_H */

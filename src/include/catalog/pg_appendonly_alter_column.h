/*-------------------------------------------------------------------------
*
* pg_appendonly_alter_column.h
*	  Internal specifications of the pg_appendonly_alter_column relation storage.
*
* Copyright (c) 2008-2010, Greenplum Inc.
*-------------------------------------------------------------------------
*/
#ifndef PG_APPENDONLY_ALTER_COLUMN_H
#define PG_APPENDONLY_ALTER_COLUMN_H

#include "catalog/genbki.h"
/*
 * pg_appendonly_alter_column definition.
 */

/* TIDYCAT_BEGINFAKEDEF

   CREATE TABLE pg_appendonly_alter_column
   with (camelcase=AppendOnlyAlterColumn, oid=false, relid=6110)
   (
   relid             oid, 
   changenum         integer, 
   segfilenums       integer[], 
   highwaterrownums  bytea
   );

   create unique index on pg_appendonly_alter_column(relid, changenum) with (indexid=5031, CamelCase=AppendOnlyAlterColumnRelid);

   alter table pg_appendonly_alter_column add fk relid on pg_class(oid);

   TIDYCAT_ENDFAKEDEF
*/

#define AppendOnlyAlterColumnRelationId  6110

CATALOG(pg_appendonly_alter_column,6110) BKI_WITHOUT_OIDS
{
	Oid			relid;		/* relation id */
	
	/* 
	 * The change number for the ALTER TABLE ADD/DROP
	 * COLUMN operation.  Starts at 1.
	 */
	int4		changenum;			

	/*
	 * THE REST OF THESE ARE VARIABLE LENGTH FIELDS.
	 * They cannot be accessed as C struct entries; you have to use
	 * the full field access machinery (heap_getattr) for them.  We declare
	 * them here for the catalog machinery.
	 */

	/* The array of int4 segment file numbers for the highwater row numbers. */
	int4		segfilenums[1];

	/* 
	 * The array of int64 row numbers that are the logical end of the segment
	 * files. Unfortunately, we cannot properly represent it as an array of
	 * int8 or float8, so we use raw bytes.
	 */
	bytea		highwaterrownums;		
} FormData_pg_appendonly_alter_column;


/* ----------------
*		Form_pg_appendonly corresponds to a pointer to a tuple with
*		the format of pg_appendonly relation.
* ----------------
*/
typedef FormData_pg_appendonly_alter_column *Form_pg_appendonly_alter_column;

#define Natts_pg_appendonly_alter_column					4
#define Anum_pg_appendonly_alter_column_relid				1
#define Anum_pg_appendonly_alter_column_changenum			2
#define Anum_pg_appendonly_alter_column_segfilenums			3
#define Anum_pg_appendonly_alter_column_highwaterrownums	4

#endif   /* PG_APPENDONLY_ALTER_COLUMN_H */


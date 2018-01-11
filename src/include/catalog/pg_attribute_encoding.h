/*-------------------------------------------------------------------------
 *
 * pg_attribute_encoding.h
 *	  some where to stash column level ENCODING () clauses
 *
 * Portions Copyright (c) EMC, 2011
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/pg_attribute_encoding.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_ATTRIBUTE_ENCODING_H
#define PG_ATTRIBUTE_ENCODING_H

#include "catalog/genbki.h"
#include "utils/rel.h"

/* ----------------
 *		pg_attribute_encoding definition.  cpp turns this into
 *		typedef struct FormData_pg_attribute_encoding
 * ----------------
 */
#define AttributeEncodingRelationId	3231

CATALOG(pg_attribute_encoding,3231) BKI_WITHOUT_OIDS
{
	Oid		attrelid;		
	int2	attnum;			
	text	attoptions[1];	
} FormData_pg_attribute_encoding;

/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(attrelid REFERENCES pg_attribute(attrelid));

/* ----------------
 *		Form_pg_attribute_encoding corresponds to a pointer to a tuple with
 *		the format of pg_attribute_encoding relation.
 * ----------------
 */
typedef FormData_pg_attribute_encoding *Form_pg_attribute_encoding;


/* ----------------
 *		compiler constants for pg_attribute_encoding
 * ----------------
 */
#define Natts_pg_attribute_encoding				3
#define Anum_pg_attribute_encoding_attrelid		1
#define Anum_pg_attribute_encoding_attnum		2
#define Anum_pg_attribute_encoding_attoptions	3

extern PGFunction *get_funcs_for_compression(char *compresstype);
extern StdRdOptions **RelationGetAttributeOptions(Relation rel);
extern List **RelationGetUntransformedAttributeOptions(Relation rel);

extern void AddRelationAttributeEncodings(Relation rel, List *attr_encodings);
extern void RemoveAttributeEncodingsByRelid(Oid relid);
extern void cloneAttributeEncoding(Oid oldrelid, Oid newrelid, AttrNumber max_attno);
extern Datum *get_rel_attoptions(Oid relid, AttrNumber max_attno);
extern void AddDefaultRelationAttributeOptions(Relation rel, List *options);

#endif   /* PG_ATTRIBUTE_ENCODING_H */

/*-------------------------------------------------------------------------
 *
 * pg_attribute_encoding.h
 *	  some where to stash column level ENCODING () clauses
 *
 * GPDB_90_MERGE_FIXME: pg_attribute now has an attoptions field. We should
 * get rid of this table, and start using pg_attribute.attoptions instead.
 *
 * Portions Copyright (c) EMC, 2011
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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
#include "catalog/pg_attribute_encoding_d.h"
#include "utils/rel.h"

/* ----------------
 *		pg_attribute_encoding definition.  cpp turns this into
 *		typedef struct FormData_pg_attribute_encoding
 * ----------------
 */
CATALOG(pg_attribute_encoding,6231,AttributeEncodingRelationId)
{
	Oid		attrelid;		
	int16	attnum;			
#ifdef CATALOG_VARLEN			/* variable-length fields start here */
	text	attoptions[1];	
#endif
} FormData_pg_attribute_encoding;

/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(attrelid REFERENCES pg_attribute(attrelid));

/* ----------------
 *		Form_pg_attribute_encoding corresponds to a pointer to a tuple with
 *		the format of pg_attribute_encoding relation.
 * ----------------
 */
typedef FormData_pg_attribute_encoding *Form_pg_attribute_encoding;


extern PGFunction *get_funcs_for_compression(char *compresstype);
extern StdRdOptions **RelationGetAttributeOptions(Relation rel);
extern List **RelationGetUntransformedAttributeOptions(Relation rel);

extern void AddRelationAttributeEncodings(Relation rel, List *attr_encodings);
extern void RemoveAttributeEncodingsByRelid(Oid relid);
extern void cloneAttributeEncoding(Oid oldrelid, Oid newrelid, AttrNumber max_attno);
extern Datum *get_rel_attoptions(Oid relid, AttrNumber max_attno);

#endif   /* PG_ATTRIBUTE_ENCODING_H */

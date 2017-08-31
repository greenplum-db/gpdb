/*-------------------------------------------------------------------------
 *
 * pg_type_encoding.h
 *	  some where to stash type ENCODING () clauses
 *
 * Portions Copyright (c) EMC, 2011
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/pg_type_encoding.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_TYPE_ENCODING_H
#define PG_TYPE_ENCODING_H

#include "catalog/genbki.h"

/* ----------------
 *		pg_type_encoding definition.  cpp turns this into
 *		typedef struct FormData_pg_type_encoding
 * ----------------
 */
#define TypeEncodingRelationId	3220

CATALOG(pg_type_encoding,3220) BKI_WITHOUT_OIDS
{
	Oid		typid;			
	text	typoptions[1];	
} FormData_pg_type_encoding;

/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(typid REFERENCES pg_type(oid));

/* ----------------
 *		Form_pg_type_encoding corresponds to a pointer to a tuple with
 *		the format of pg_type_encoding relation.
 * ----------------
 */
typedef FormData_pg_type_encoding *Form_pg_type_encoding;


/* ----------------
 *		compiler constants for pg_type_encoding
 * ----------------
 */
#define Natts_pg_type_encoding				2
#define Anum_pg_type_encoding_typid			1
#define Anum_pg_type_encoding_typoptions	2

#endif   /* PG_TYPE_ENCODING_H */

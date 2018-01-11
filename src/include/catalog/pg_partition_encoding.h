/*-------------------------------------------------------------------------
 *
 * pg_partition_encoding.h
 *	  some where to stash ENCODING() clauses for partition templates
 *
 * Portions Copyright (c) EMC, 2011
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/pg_partition_encoding.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_PARTITION_ENCODING_H
#define PG_PARTITION_ENCODING_H

#include "catalog/genbki.h"

/* ----------------
 *		pg_partition_encoding definition.  cpp turns this into
 *		typedef struct FormData_pg_partition_encoding
 * ----------------
 */
#define PartitionEncodingRelationId	9903

CATALOG(pg_partition_encoding,9903) BKI_WITHOUT_OIDS
{
	Oid		parencoid;				
	int2	parencattnum;			
	text	parencattoptions[1];	
} FormData_pg_partition_encoding;

/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(parencoid REFERENCES pg_partition(oid));

/* ----------------
 *		Form_pg_partition_encoding corresponds to a pointer to a tuple with
 *		the format of pg_partition_encoding relation.
 * ----------------
 */
typedef FormData_pg_partition_encoding *Form_pg_partition_encoding;


/* ----------------
 *		compiler constants for pg_partition_encoding
 * ----------------
 */
#define Natts_pg_partition_encoding					3
#define Anum_pg_partition_encoding_parencoid		1
#define Anum_pg_partition_encoding_parencattnum		2
#define Anum_pg_partition_encoding_parencattoptions	3

#endif   /* PG_PARTITION_ENCODING_H */

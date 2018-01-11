/*-------------------------------------------------------------------------
 *
 * pg_partition.h
 *	  Internal specifications of the partition configuration.
 *
 * Portions Copyright (c) 2008-2010, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/pg_partition.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_PARTITION_H
#define PG_PARTITION_H


#include "catalog/genbki.h"

/* ----------------
 *		pg_partition definition.  cpp turns this into
 *		typedef struct FormData_pg_partition
 * ----------------
 */
#define PartitionRelationId	5010

CATALOG(pg_partition,5010)
{
	Oid			parrelid;		
	char		parkind;		
	int2		parlevel;		
	bool		paristemplate;	
	int2		parnatts;		
	int2vector	paratts;		
	oidvector	parclass;		
} FormData_pg_partition;

/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(parrelid REFERENCES pg_class(oid));
/* alter table pg_partition add vector_fk parclass on pg_opclass(oid); */


/* ----------------
 *		Form_pg_partition corresponds to a pointer to a tuple with
 *		the format of pg_partition relation.
 * ----------------
 */
typedef FormData_pg_partition *Form_pg_partition;


/* ----------------
 *		compiler constants for pg_partition
 * ----------------
 */
#define Natts_pg_partition				7
#define Anum_pg_partition_parrelid		1
#define Anum_pg_partition_parkind		2
#define Anum_pg_partition_parlevel		3
#define Anum_pg_partition_paristemplate	4
#define Anum_pg_partition_parnatts		5
#define Anum_pg_partition_paratts		6
#define Anum_pg_partition_parclass		7

#endif   /* PG_PARTITION_H */

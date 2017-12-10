/*-------------------------------------------------------------------------
 *
 * gp_global_sequence.h
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/gp_global_sequence.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef GP_GLOBAL_SEQUENCE_H
#define GP_GLOBAL_SEQUENCE_H

#include "catalog/genbki.h"

/*
 * Defines for gp_global_sequence table
 */
#define GpGlobalSequenceRelationName	"gp_global_sequence"

#define GpGlobalSequenceRelationId 5096

CATALOG(gp_global_sequence,5096) BKI_SHARED_RELATION BKI_WITHOUT_OIDS
{
	int8		sequence_num;
} FormData_gp_global_sequence;

/* no foreign keys */

#define Natts_gp_global_sequence				    1
#define Anum_gp_global_sequence_sequence_num     	1
 
typedef FormData_gp_global_sequence *Form_gp_global_sequence;


/*
 * gp_global_sequence table values for FormData_pg_attribute.
 *
 * [Similar examples are Schema_pg_type, Schema_pg_proc, Schema_pg_attribute, etc, in
 *  pg_attribute.h]
 */
#define Schema_gp_global_sequence \
{ GpGlobalSequenceRelationId, {"sequence_num"},	20, -1, 8, 1, 0, -1, -1, true, 'p', 'd', true, false, false, true, 0 }

/*
 * gp_global_sequence table values for FormData_pg_class.
 */
#define Class_gp_global_sequence \
  {"gp_global_sequence"}, PG_CATALOG_NAMESPACE, GP_GLOBAL_SEQUENCE_RELTYPE_OID, BOOTSTRAP_SUPERUSERID, 0, \
               GpGlobalSequenceRelationId, GLOBALTABLESPACE_OID, \
               25, 10000, 0, 0, false, true, false, RELKIND_RELATION, RELSTORAGE_HEAP, Natts_gp_global_sequence, \
               0, false, false, false, false, false, FirstNormalTransactionId, {0}, {{{'\0','\0','\0','\0'},{'\0'}}}


#undef int8

/*
 * The assigned sequence counters.
 */
typedef enum GpGlobalSequence
{
	GpGlobalSequence_PersistentRelation = 1,	// Must start at 1, since tuple item of 0 are invalid.
	GpGlobalSequence_PersistentDatabase = 2,
	GpGlobalSequence_PersistentTablespace = 3,
	GpGlobalSequence_PersistentFilespace = 4,
} GpGlobalSequence;

/*
 * This is the number of rows that the gp_global_sequence sequence table
 * should have, they should be a contiguous set of tids from (0,1)-(0,N)
 *
 * This is populated during the gp_persistent_build_db() function rather
 * than as part of normal dbinit operations.
 */
#define GpGlobalSequence_MaxSequenceTid 15

extern void GpGlobalSequence_GetValues(
	Datum						*values,

	int64						*sequenceNum);

extern void GpGlobalSequence_SetDatumValues(
	Datum					*values,

	int64					sequenceNum);

#endif   /* GP_GLOBAL_SEQUENCE_H */

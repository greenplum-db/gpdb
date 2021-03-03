/*-------------------------------------------------------------------------
 *
 * gp_fastsequence.h
 *    a table maintaining a light-weight fast sequence number for a unique
 *    object.
 *
 * Portions Copyright (c) 2009-2011, Greenplum Inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/gp_fastsequence.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef GP_FASTSEQUENCE_H
#define GP_FASTSEQUENCE_H

#include "catalog/genbki.h"
#include "catalog/gp_fastsequence_d.h"

/*
 * gp_fastsequence definition
 */
CATALOG(gp_fastsequence,5043,FastSequenceRelationId)
{
	Oid				objid;				/* object oid */
	int8			objmod;				/* object modifier */
	int8			last_sequence;      /* the last sequence number used by the object */
} FormData_gp_fastsequence;


/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(objid REFERENCES pg_class(oid));

/* ----------------
*		Form_gp_fastsequence corresponds to a pointer to a tuple with
*		the format of gp_fastsequence relation.
* ----------------
*/
typedef FormData_gp_fastsequence *Form_gp_fastsequence;

#define NUM_FAST_SEQUENCES					 100

/*
 * Insert a new light-weight fast sequence entry for a given object.
 */
extern void InsertFastSequenceEntry(Oid objid, int64 objmod,
									int64 lastSequence);


extern void InsertInitialFastSequenceEntries(Oid objid);

/*
 * GetFastSequences
 *
 * Get a list of consecutive sequence numbers. The starting sequence
 * number is the maximal value between 'lastsequence' + 1 and minSequence.
 * The length of the list is given.
 *
 * If there is not such an entry for objid in the table, create
 * one here.
 *
 * The existing entry for objid in the table is updated with a new
 * lastsequence value.
 */
extern int64 GetFastSequences(Oid objid, int64 objmod,
							  int64 minSequence, int64 numSequences);

extern int64 ReadLastSequence(Oid objid, int64 objmod);

/*
 * RemoveFastSequenceEntry
 *
 * Remove all entries associated with the given object id.
 *
 * If the given objid is an invalid OID, this function simply
 * returns.
 *
 * If the given valid objid does not have an entry in
 * gp_fastsequence, this function errors out.
 */
extern void RemoveFastSequenceEntry(Oid objid);

#endif

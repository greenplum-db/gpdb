/*-------------------------------------------------------------------------
 *
 * aoseg.h
 *	  This file provides some definitions to support creation of aoseg tables
 *
 * Portions Copyright (c) 2008, Greenplum Inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/aoseg.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef AOSEG_H
#define AOSEG_H

#include "access/appendonlytid.h"
#include "storage/lock.h"
#include "gp_fastsequence.h"

/*
 * aoseg.c prototypes
 */
extern void AlterTableCreateAoSegTable(Oid relOid);

/*
 * Given the aosegrel oid and segno for an append-optimized table, populate the
 * provided BlockSequence.
 */
static inline void
AOSegment_PopulateBlockSequence(BlockSequence *sequence,
								Oid segrelid,
								int segno)
{
	int64 lastSequence = ReadLastSequence(segrelid, segno);

	Assert(sequence);
	Assert(OidIsValid(segrelid));
	Assert(segno >= 0 && segno <= AOTupleId_MaxSegmentFileNum);

	sequence->startblknum = AOSegmentGet_startHeapBlock(segno);
	FastSequenceGetNumHeapBlocks(lastSequence, &sequence->nblocks);
}

#endif   /* AOSEG_H */

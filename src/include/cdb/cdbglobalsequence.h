/*-------------------------------------------------------------------------
 *
 * cdbglobalsequence.h
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbglobalsequence.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBGLOBALSEQUENCE_H
#define CDBGLOBALSEQUENCE_H

#include "catalog/gp_global_sequence.h"

int64 GlobalSequence_Current(
	GpGlobalSequence		gpGlobalSequence);

void GlobalSequence_Set(
	GpGlobalSequence		gpGlobalSequence,

	int64					newSequenceNum);

#endif   /* CDBGLOBALSEQUENCE_H */

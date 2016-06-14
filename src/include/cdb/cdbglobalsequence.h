/*-------------------------------------------------------------------------
 *
 * cdbglobalsequence.h
 *
 * Copyright (c) 2009-2010, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBGLOBALSEQUENCE_H
#define CDBGLOBALSEQUENCE_H

#include "catalog/gp_global_sequence.h"

int64 GlobalSequence_Next(
	GpGlobalSequence		gpGlobalSequence);

int64 GlobalSequence_NextInterval(
	GpGlobalSequence		gpGlobalSequence,

	int64					interval);

int64 GlobalSequence_Current(
	GpGlobalSequence		gpGlobalSequence);

void GlobalSequence_Set(
	GpGlobalSequence		gpGlobalSequence,

	int64					newSequenceNum);

#endif   /* CDBGLOBALSEQUENCE_H */

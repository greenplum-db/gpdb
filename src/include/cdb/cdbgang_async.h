/*-------------------------------------------------------------------------
 *
 * cdbgang_async.h
 *	  Routines for asynchronous implementation of creating gang.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbgang_async.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBGANG_ASYNC_H
#define CDBGANG_ASYNC_H

#include "cdb/cdbgang.h"

extern Gang *cdbgang_createGang_async(List *segments, SegmentType segmentType);

#endif

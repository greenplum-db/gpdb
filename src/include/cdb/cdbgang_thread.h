/*-------------------------------------------------------------------------
 *
 * cdbdisp_gang.h
 * routines for multi-thread implementation of creating gang
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbgang_thread.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBGANG_THREAD_H
#define CDBGANG_THREAD_H

#include "cdb/cdbgang.h"

extern CreateGangFunc pCreateGangFuncThreaded;

#endif

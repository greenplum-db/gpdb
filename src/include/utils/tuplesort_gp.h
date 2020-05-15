/*-------------------------------------------------------------------------
 *
 * tuplesort_gp.h
 *	  Public API to GPDB tuple sorting routines.
 *
 * NOTES
 *    This file contains additional Greenplum-added entries into tuplesort.h,
 *    on top of the upstream entries from tuplesort.h:
 *    - Support for reader-writer tuplesort
 *
 * NB: This should not be #included directly, only from tuplesort.h!
 * The switcheroo magic to switch between the normal and MK tuplesorts
 * might get confused otherwise.
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/tuplesort_gp.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef TUPLESORT_GP_H
#define TUPLESORT_GP_H

#include "utils/tuplesort.h"
#include "nodes/execnodes.h"
#include "utils/workfile_mgr.h"

/*
 *-------------------------------------------------------------------------
 * TupleSort's GPDB related API.
 *-------------------------------------------------------------------------
 */
struct Instrumentation;                 /* #include "executor/instrument.h" */
struct StringInfoData;                  /* #include "lib/stringinfo.h" */

/* 
 * TuplesortState and TuplesortPos are opaque types whose details are not known
 * outside tuplesort.c.
 */
struct Tuplesortstate;
struct ScanState;

extern void cdb_tuplesort_init(struct Tuplesortstate *state, int unique,
							   int sort_flags,
							   int64 maxdistinct);

extern void tuplesort_finalize_stats(struct Tuplesortstate *state);

/*
 * tuplesort_set_instrument
 *
 * May be called after tuplesort_begin_xxx() to enable reporting of
 * statistics and events for EXPLAIN ANALYZE.
 *
 * The 'instr' and 'explainbuf' ptrs are retained in the 'state' object for
 * possible use anytime during the sort, up to and including tuplesort_end().
 * The caller must ensure that the referenced objects remain allocated and
 * valid for the life of the Tuplestorestate object; or if they are to be
 * freed early, disconnect them by calling again with NULL pointers.
 */
extern void tuplesort_set_instrument(struct Tuplesortstate *state,
                         struct Instrumentation    *instrument,
                         struct StringInfoData     *explainbuf);

#endif   /* TUPLESORT_GP_H */


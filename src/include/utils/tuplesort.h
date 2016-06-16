/*-------------------------------------------------------------------------
 *
 * tuplesort.h
 *	  Generalized tuple sorting routines.
 *
 * This module handles sorting of heap tuples, index tuples, or single
 * Datums (and could easily support other kinds of sortable objects,
 * if necessary).  It works efficiently for both small and large amounts
 * of data.  Small amounts are sorted in-memory using qsort().	Large
 * amounts are sorted using temporary files and a standard external sort
 * algorithm.
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/utils/tuplesort.h,v 1.28 2008/01/01 19:45:59 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef TUPLESORT_H
#define TUPLESORT_H

#include "access/itup.h"
#include "executor/tuptable.h"
#include "nodes/execnodes.h"
#include "utils/workfile_mgr.h"

#include "gpmon/gpmon.h"

struct Instrumentation;                 /* #include "executor/instrument.h" */
struct StringInfoData;                  /* #include "lib/stringinfo.h" */

/* Tuplesortstate and TuplesortPos are opaque types whose details are not known outside
 * tuplesort.c.
 */
typedef struct TuplesortPos TuplesortPos;
typedef struct Tuplesortstate Tuplesortstate;
typedef struct TuplesortPos_mk TuplesortPos_mk;
typedef struct Tuplesortstate_mk Tuplesortstate_mk;

/*
 * We provide two different interfaces to what is essentially the same
 * code: one for sorting HeapTuples and one for sorting IndexTuples.
 * They differ primarily in the way that the sort key information is
 * supplied.  Also, the HeapTuple case actually stores MinimalTuples,
 * which means it doesn't preserve the "system columns" (tuple identity and
 * transaction visibility info).  The IndexTuple case does preserve all
 * the header fields of an index entry.  In the HeapTuple case we can
 * save some cycles by passing and returning the tuples in TupleTableSlots,
 * rather than forming actual HeapTuples (which'd have to be converted to
 * MinimalTuples).
 *
 * Yet a third slightly different interface supports sorting bare Datums.
 */
extern void tuplesort_begin_pos(Tuplesortstate *state, TuplesortPos **pos);
extern void tuplesort_begin_pos_mk(Tuplesortstate_mk *state, TuplesortPos_mk **pos);

extern Tuplesortstate *tuplesort_begin_heap(TupleDesc tupDesc,
					 int nkeys, AttrNumber *attNums,
					 Oid *sortOperators, bool *nullsFirstFlags,
					 int workMem, bool randomAccess);
extern Tuplesortstate_mk *tuplesort_begin_heap_mk(ScanState * ss,
					 TupleDesc tupDesc,
					 int nkeys, AttrNumber *attNums,
					 Oid *sortOperators, bool *nullsFirstFlags,
					 int workMem, bool randomAccess);


extern Tuplesortstate *tuplesort_begin_heap_file_readerwriter(
		const char* rwfile_prefix, bool isWriter,
		TupleDesc tupDesc, 
		int nkeys, AttrNumber *attNums,
		Oid *sortOperators, bool *nullsFirstFlags,
		int workMem, bool randomAccess);
extern Tuplesortstate_mk *tuplesort_begin_heap_file_readerwriter_mk(
		ScanState * ss,
		const char* rwfile_prefix, bool isWriter,
		TupleDesc tupDesc, 
		int nkeys, AttrNumber *attNums,
		Oid *sortOperators, bool *nullsFirstFlags,
		int workMem, bool randomAccess);

extern Tuplesortstate *tuplesort_begin_index(Relation indexRel,
					  bool enforceUnique,
					  int workMem, bool randomAccess);
extern Tuplesortstate_mk *tuplesort_begin_index_mk(Relation indexRel,
					  bool enforceUnique,
					  int workMem, bool randomAccess);

extern Tuplesortstate *tuplesort_begin_datum(Oid datumType,
					  Oid sortOperator, bool nullsFirstFlag,
					  int workMem, bool randomAccess);
extern Tuplesortstate_mk *tuplesort_begin_datum_mk(ScanState * ss,
					  Oid datumType,
					  Oid sortOperator, bool nullsFirstFlag,
					  int workMem, bool randomAccess);

extern void cdb_tuplesort_init(Tuplesortstate *state, 
							   int64 offset, int64 limit, int unique,
							   int sort_flags,
							   int64 maxdistinct);
extern void cdb_tuplesort_init_mk(Tuplesortstate_mk *state, 
							   int64 offset, int64 limit, int unique,
							   int sort_flags,
							   int64 maxdistinct);

extern void tuplesort_set_bound(Tuplesortstate *state, int64 bound);
extern void tuplesort_set_bound_mk(Tuplesortstate_mk *state, int64 bound);

extern void tuplesort_puttupleslot(Tuplesortstate *state, TupleTableSlot *slot);
extern void tuplesort_puttupleslot_mk(Tuplesortstate_mk *state, TupleTableSlot *slot);

extern void tuplesort_putindextuple(Tuplesortstate *state, IndexTuple tuple);
extern void tuplesort_putindextuple_mk(Tuplesortstate_mk *state, IndexTuple tuple);

extern void tuplesort_putdatum(Tuplesortstate *state, Datum val, bool isNull);
extern void tuplesort_putdatum_mk(Tuplesortstate_mk *state, Datum val, bool isNull);

extern void tuplesort_performsort(Tuplesortstate *state);
extern void tuplesort_performsort_mk(Tuplesortstate_mk *state);

extern bool tuplesort_gettupleslot_pos(Tuplesortstate *state, TuplesortPos *pos, bool forward, TupleTableSlot *slot, MemoryContext mcontext);
extern bool tuplesort_gettupleslot_pos_mk(Tuplesortstate_mk *state, TuplesortPos_mk *pos, bool forward, TupleTableSlot *slot, MemoryContext mcontext);

extern bool tuplesort_gettupleslot(Tuplesortstate *state, bool forward, TupleTableSlot *slot);
extern bool tuplesort_gettupleslot_mk(Tuplesortstate_mk *state, bool forward, TupleTableSlot *slot);

extern IndexTuple tuplesort_getindextuple(Tuplesortstate *state, bool forward, bool *should_free);
extern IndexTuple tuplesort_getindextuple_mk(Tuplesortstate_mk *state, bool forward, bool *should_free);

extern bool tuplesort_getdatum(Tuplesortstate *state, bool forward, Datum *val, bool *isNull);
extern bool tuplesort_getdatum_mk(Tuplesortstate_mk *state, bool forward, Datum *val, bool *isNull);

extern void tuplesort_end(Tuplesortstate *state);
extern void tuplesort_end_mk(Tuplesortstate_mk *state);
extern void tuplesort_flush(Tuplesortstate *state);
extern void tuplesort_flush_mk(Tuplesortstate_mk *state);

extern void tuplesort_finalize_stats(Tuplesortstate *state);
extern void tuplesort_finalize_stats_mk(Tuplesortstate_mk *state);

extern char *tuplesort_explain(Tuplesortstate *state);
extern char *tuplesort_explain_mk(Tuplesortstate_mk *state);

extern int	tuplesort_merge_order(long allowedMem);

/*
 * These routines may only be called if randomAccess was specified 'true'.
 * Likewise, backwards scan in gettuple/getdatum is only allowed if
 * randomAccess was specified.
 */

extern void tuplesort_rescan_pos(Tuplesortstate *state, TuplesortPos *pos);
extern void tuplesort_rescan_pos_mk(Tuplesortstate_mk *state, TuplesortPos_mk *pos);
extern void tuplesort_rescan(Tuplesortstate *state);
extern void tuplesort_rescan_mk(Tuplesortstate_mk *state);
extern void tuplesort_markpos(Tuplesortstate *state);
extern void tuplesort_markpos_mk(Tuplesortstate_mk *state);
extern void tuplesort_markpos_pos(Tuplesortstate *state, TuplesortPos *pos);
extern void tuplesort_markpos_pos_mk(Tuplesortstate_mk *state, TuplesortPos_mk *pos);
extern void tuplesort_restorepos(Tuplesortstate *state);
extern void tuplesort_restorepos_mk(Tuplesortstate_mk *state);
extern void tuplesort_restorepos_pos(Tuplesortstate *state, TuplesortPos *pos);
extern void tuplesort_restorepos_pos_mk(Tuplesortstate_mk *state, TuplesortPos_mk *pos);

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
extern void tuplesort_set_instrument(Tuplesortstate *state,
                         struct Instrumentation    *instrument,
                         struct StringInfoData     *explainbuf);

extern void tuplesort_set_instrument_mk(Tuplesortstate_mk *state,
                         struct Instrumentation    *instrument,
                         struct StringInfoData     *explainbuf);

/* Setup for ApplySortFunction */
extern void SelectSortFunction(Oid sortOperator, bool nulls_first,
				   Oid *sortFunction,
				   int *sortFlags);

/*
 * Apply a sort function (by now converted to fmgr lookup form)
 * and return a 3-way comparison result.  This takes care of handling
 * reverse-sort and NULLs-ordering properly.
 */
extern int32 ApplySortFunction(FmgrInfo *sortFunction, int sortFlags,
				  Datum datum1, bool isNull1,
				  Datum datum2, bool isNull2);

/* Gpmon */
extern void 
tuplesort_set_gpmon(Tuplesortstate *state,
					gpmon_packet_t *gpmon_pkt,
					int *gpmon_tick);
extern void 
tuplesort_set_gpmon_mk(Tuplesortstate_mk *state,
					gpmon_packet_t *gpmon_pkt,
					int *gpmon_tick);

extern void 
tuplesort_checksend_gpmonpkt(gpmon_packet_t *pkt, int *tick);

#endif   /* TUPLESORT_H */

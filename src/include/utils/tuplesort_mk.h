/*-------------------------------------------------------------------------
 *
 * tuplesort_mk.h
 *	  Public API to GPDB multi-key tuple sorting routines.
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/tuplesort_mk.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef TUPLESORT_MK_H
#define TUPLESORT_MK_H

#include "nodes/execnodes.h"
#include "utils/workfile_mgr.h"

typedef struct TuplesortPos_mk TuplesortPos_mk;
typedef struct Tuplesortstate_mk Tuplesortstate_mk;
struct ScanState;

extern Tuplesortstate_mk *tuplesort_begin_heap_mk(struct ScanState * ss,
					 TupleDesc tupDesc,
					 int nkeys, AttrNumber *attNums,
					 Oid *sortOperators, Oid *sortCollations,
					 bool *nullsFirstFlags,
					 int workMem, bool randomAccess);

extern Tuplesortstate_mk *tuplesort_begin_cluster_mk(TupleDesc tupDesc,
						Relation indexRel,
						int workMem, bool randomAccess);

extern Tuplesortstate_mk *tuplesort_begin_index_mk(Relation heapRel,
					  Relation indexRel,
					  bool enforceUnique,
					  int workMem, bool randomAccess);

extern Tuplesortstate_mk *tuplesort_begin_datum_mk(struct ScanState * ss,
					  Oid datumType,
					  Oid sortOperator, Oid sortCollation,
					  bool nullsFirstFlag,
					  int workMem, bool randomAccess);

extern void cdb_tuplesort_init_mk(Tuplesortstate_mk *state, int unique,
							   int sort_flags,
							   int64 maxdistinct);

extern void tuplesort_set_bound_mk(Tuplesortstate_mk *state, int64 bound);

extern void tuplesort_puttupleslot_mk(Tuplesortstate_mk *state, TupleTableSlot *slot);
extern void tuplesort_putheaptuple_mk(Tuplesortstate_mk *state, HeapTuple tup);
extern void tuplesort_putindextuplevalues_mk(Tuplesortstate_mk *state, Relation rel,
											 ItemPointer self, Datum *values, bool *isnull);
extern void tuplesort_putdatum_mk(Tuplesortstate_mk *state, Datum val, bool isNull);

extern void tuplesort_performsort_mk(Tuplesortstate_mk *state);

extern bool tuplesort_gettupleslot_mk(Tuplesortstate_mk *state, bool forward, TupleTableSlot *slot, Datum *abbrev);
extern HeapTuple tuplesort_getheaptuple_mk(Tuplesortstate_mk *state, bool forward, bool *should_free);
extern IndexTuple tuplesort_getindextuple_mk(Tuplesortstate_mk *state, bool forward, bool *should_free);
extern bool tuplesort_getdatum_mk(Tuplesortstate_mk *state, bool forward, Datum *val, bool *isNull, Datum *abbrev);
extern bool tuplesort_skiptuples_mk(Tuplesortstate_mk *state, int64 ntuples, bool forward);

extern void tuplesort_end_mk(Tuplesortstate_mk *state);
extern void tuplesort_flush_mk(Tuplesortstate_mk *state);
extern void tuplesort_finalize_stats_mk(Tuplesortstate_mk *state);


extern void tuplesort_rescan_mk(Tuplesortstate_mk *state);
extern void tuplesort_markpos_mk(Tuplesortstate_mk *state);
extern void tuplesort_restorepos_mk(Tuplesortstate_mk *state);

extern void tuplesort_set_instrument_mk(Tuplesortstate_mk *state,
                         struct Instrumentation    *instrument,
                         struct StringInfoData     *explainbuf);

#endif   /* TUPLESORT_MK_H */

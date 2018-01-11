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
#include "gpmon/gpmon.h"


typedef struct TuplesortPos_mk TuplesortPos_mk;
typedef struct Tuplesortstate_mk Tuplesortstate_mk;


extern Tuplesortstate_mk *tuplesort_begin_heap_mk(ScanState * ss,
					 TupleDesc tupDesc,
					 int nkeys, AttrNumber *attNums,
					 Oid *sortOperators, bool *nullsFirstFlags,
					 int workMem, bool randomAccess);

extern Tuplesortstate_mk *tuplesort_begin_index_mk(Relation indexRel,
					  bool enforceUnique,
					  int workMem, bool randomAccess);

extern Tuplesortstate_mk *tuplesort_begin_datum_mk(ScanState * ss,
					  Oid datumType,
					  Oid sortOperator, bool nullsFirstFlag,
					  int workMem, bool randomAccess);

extern Tuplesortstate_mk *tuplesort_begin_heap_file_readerwriter_mk(
		ScanState * ss,
		const char* rwfile_prefix, bool isWriter,
		TupleDesc tupDesc,
		int nkeys, AttrNumber *attNums,
		Oid *sortOperators, bool *nullsFirstFlags,
		int workMem, bool randomAccess);

extern void cdb_tuplesort_init_mk(Tuplesortstate_mk *state, int unique,
							   int sort_flags,
							   int64 maxdistinct);

extern void tuplesort_set_bound_mk(Tuplesortstate_mk *state, int64 bound);

extern void tuplesort_puttupleslot_mk(Tuplesortstate_mk *state, TupleTableSlot *slot);
extern void tuplesort_putindextuple_mk(Tuplesortstate_mk *state, IndexTuple tuple);
extern void tuplesort_putdatum_mk(Tuplesortstate_mk *state, Datum val, bool isNull);

extern void tuplesort_performsort_mk(Tuplesortstate_mk *state);

extern void tuplesort_begin_pos_mk(Tuplesortstate_mk *state, TuplesortPos_mk **pos);
extern bool tuplesort_gettupleslot_pos_mk(Tuplesortstate_mk *state, TuplesortPos_mk *pos, bool forward, TupleTableSlot *slot, MemoryContext mcontext);

extern bool tuplesort_gettupleslot_mk(Tuplesortstate_mk *state, bool forward, TupleTableSlot *slot);
extern IndexTuple tuplesort_getindextuple_mk(Tuplesortstate_mk *state, bool forward, bool *should_free);
extern bool tuplesort_getdatum_mk(Tuplesortstate_mk *state, bool forward, Datum *val, bool *isNull);
extern bool tuplesort_skiptuples_mk(Tuplesortstate_mk *state, int64 ntuples, bool forward);

extern void tuplesort_end_mk(Tuplesortstate_mk *state);
extern void tuplesort_flush_mk(Tuplesortstate_mk *state);
extern void tuplesort_finalize_stats_mk(Tuplesortstate_mk *state);


extern void tuplesort_rescan_mk(Tuplesortstate_mk *state);
extern void tuplesort_markpos_mk(Tuplesortstate_mk *state);
extern void tuplesort_restorepos_mk(Tuplesortstate_mk *state);

extern void tuplesort_rescan_pos_mk(Tuplesortstate_mk *state, TuplesortPos_mk *pos);
extern void tuplesort_restorepos_pos_mk(Tuplesortstate_mk *state, TuplesortPos_mk *pos);
extern void tuplesort_markpos_pos_mk(Tuplesortstate_mk *state, TuplesortPos_mk *pos);


extern void tuplesort_set_instrument_mk(Tuplesortstate_mk *state,
                         struct Instrumentation    *instrument,
                         struct StringInfoData     *explainbuf);

extern void
tuplesort_set_gpmon_mk(Tuplesortstate_mk *state,
					gpmon_packet_t *gpmon_pkt,
					int *gpmon_tick);


#endif   /* TUPLESORT_MK_H */

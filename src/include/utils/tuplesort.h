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
 * GPDB has two implemntations of sorting. One is inherited from PostgreSQL,
 * and lives in tuplesort.c. The other one lives in tuplesort_mk.c. Both
 * provide the same API, and have the same set of functions, just with a
 * different suffix.
 *
 * The rest of the system uses functions named tuplesort_*(), to perform
 * sorting. But there are actually three versions of each of these functions:
 *
 * tuplesort_*_pg			- the Postgres implementation
 * tuplesort_*_mk			- the Multi-Key implementation
 * switcheroo_tuplesort_*	- wrapper that calls one of the above.
 *
 * This system is fairly robust, even if new functions are added in the
 * upstream, and we merge that code in. If you don't add the #defines
 * for the new functions, you will get compiler warnings from any callers
 * of them, as the callers will pass a switcheroo_Tuplesortstate struct
 * as the argument, instead of Tuplesortstate_pk.
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/utils/tuplesort.h,v 1.32 2009/01/01 17:24:02 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef TUPLESORT_H
#define TUPLESORT_H

/*
 * First, declare the prototypes for the tuplesort_*_pg functions. In order to
 * keeping this file unchanged from the upstream, the extern declarations
 * use just tuplesort_*(), but we map them with #define to tuplesort_*_pg().
 *
 * The tuplesort_*_mk() functions are declare normally, without #defines,
 * in tuplesort_mk.h
 */

/* these are in tuplesort.h */
#define Tuplesortstate Tuplesortstate_pg

#define tuplesort_begin_heap tuplesort_begin_heap_pg
#define tuplesort_begin_index_btree tuplesort_begin_index_btree_pg
#define tuplesort_begin_index_hash tuplesort_begin_index_hash_pg
#define tuplesort_begin_datum tuplesort_begin_datum_pg
#define tuplesort_set_bound tuplesort_set_bound_pg
#define tuplesort_puttupleslot tuplesort_puttupleslot_pg
#define tuplesort_putindextuple tuplesort_putindextuple_pg
#define tuplesort_putdatum tuplesort_putdatum_pg
#define tuplesort_performsort tuplesort_performsort_pg
#define tuplesort_gettupleslot tuplesort_gettupleslot_pg
#define tuplesort_getindextuple tuplesort_getindextuple_pg
#define tuplesort_getdatum tuplesort_getdatum_pg
#define tuplesort_skiptuples tuplesort_skiptuples_pg
#define tuplesort_end tuplesort_end_pg
/* tuplesort_merge_order not switched */
#define tuplesort_rescan tuplesort_rescan_pg
#define tuplesort_markpos tuplesort_markpos_pg
#define tuplesort_restorepos tuplesort_restorepos_pg

/* these are in tuplesort_gp.h */
#define tuplesort_begin_heap_file_readerwriter tuplesort_begin_heap_file_readerwriter_pg
#define cdb_tuplesort_init cdb_tuplesort_init_pg
#define tuplesort_begin_pos tuplesort_begin_pos_pg
#define tuplesort_gettupleslot_pos tuplesort_gettupleslot_pos_pg
#define tuplesort_flush tuplesort_flush_pg
#define tuplesort_finalize_stats tuplesort_finalize_stats_pg
#define tuplesort_rescan_pos tuplesort_rescan_pos_pg
#define tuplesort_markpos_pos tuplesort_markpos_pos_pg
#define tuplesort_restorepos_pos tuplesort_restorepos_pos_pg
#define tuplesort_set_instrument tuplesort_set_instrument_pg
#define tuplesort_set_gpmon tuplesort_set_gpmon_pg

#include "access/itup.h"
#include "executor/tuptable.h"
#include "fmgr.h"
#include "utils/relcache.h"

#include "utils/tuplesort_gp.h"


/* Tuplesortstate is an opaque type whose details are not known outside
 * tuplesort.c.
 */
typedef struct Tuplesortstate Tuplesortstate;

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
 * The IndexTuple case is itself broken into two subcases, one for btree
 * indexes and one for hash indexes; the latter variant actually sorts
 * the tuples by hash code.  The API is the same except for the "begin"
 * routine.
 *
 * Yet another slightly different interface supports sorting bare Datums.
 */

extern Tuplesortstate *tuplesort_begin_heap(ScanState *ss, TupleDesc tupDesc,
					 int nkeys, AttrNumber *attNums,
					 Oid *sortOperators, bool *nullsFirstFlags,
					 int workMem, bool randomAccess);
extern Tuplesortstate *tuplesort_begin_index_btree(Relation indexRel,
							bool enforceUnique,
							int workMem, bool randomAccess);
extern Tuplesortstate *tuplesort_begin_index_hash(Relation indexRel,
							uint32 hash_mask,
							int workMem, bool randomAccess);
extern Tuplesortstate *tuplesort_begin_datum(ScanState *ss, Oid datumType,
					  Oid sortOperator, bool nullsFirstFlag,
					  int workMem, bool randomAccess);

extern void tuplesort_set_bound(Tuplesortstate *state, int64 bound);

extern void tuplesort_puttupleslot(Tuplesortstate *state,
					   TupleTableSlot *slot);
extern void tuplesort_putindextuple(Tuplesortstate *state, IndexTuple tuple);
extern void tuplesort_putdatum(Tuplesortstate *state, Datum val,
				   bool isNull);

extern void tuplesort_performsort(Tuplesortstate *state);

extern bool tuplesort_gettupleslot(Tuplesortstate *state, bool forward,
					   TupleTableSlot *slot);
extern IndexTuple tuplesort_getindextuple(Tuplesortstate *state, bool forward,
						bool *should_free);
extern bool tuplesort_getdatum(Tuplesortstate *state, bool forward,
				   Datum *val, bool *isNull);

extern bool tuplesort_skiptuples(Tuplesortstate *state, int64 ntuples,
					 bool forward);

extern void tuplesort_end(Tuplesortstate *state);

extern int	tuplesort_merge_order(long allowedMem);


/*
 * These routines may only be called if randomAccess was specified 'true'.
 * Likewise, backwards scan in gettuple/getdatum is only allowed if
 * randomAccess was specified.
 */

extern void tuplesort_rescan(Tuplesortstate *state);
extern void tuplesort_markpos(Tuplesortstate *state);
extern void tuplesort_restorepos(Tuplesortstate *state);

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


/*
 * We have now declared the protoypes for all the *_pg functions. If we're
 * compiling tuplesort.c itself, continue compilation with #define's in
 * place, so that the functions in tuplesort.c get defined as *_pg.
 *
 * If we're compiling anything else, define switcheroo_* functions that check
 * which implementation should be used, and redefine tuplesort_* to point to
 * the switcheroo-functions. This way, all callers of tuplesort_* get either
 * the Postgres or MK-implementation, depending on the gp_enable_mk_sort
 * GUC.
 */
#ifndef COMPILING_TUPLESORT_C

#undef tuplesort_begin_heap
#undef tuplesort_begin_index_btree
#undef tuplesort_begin_index_hash
#undef tuplesort_begin_datum
#undef tuplesort_set_bound
#undef tuplesort_puttupleslot
#undef tuplesort_putindextuple
#undef tuplesort_putdatum
#undef tuplesort_performsort
#undef tuplesort_gettupleslot
#undef tuplesort_getindextuple
#undef tuplesort_getdatum
#undef tuplesort_skiptuples
#undef tuplesort_end
#undef tuplesort_rescan
#undef tuplesort_markpos
#undef tuplesort_restorepos
#undef tuplesort_begin_heap_file_readerwriter
#undef cdb_tuplesort_init
#undef tuplesort_begin_pos
#undef tuplesort_gettupleslot_pos
#undef tuplesort_flush
#undef tuplesort_finalize_stats
#undef tuplesort_rescan_pos
#undef tuplesort_markpos_pos
#undef tuplesort_restorepos_pos
#undef tuplesort_set_instrument
#undef tuplesort_set_gpmon

#include "tuplesort_mk.h"

extern bool gp_enable_mk_sort;

/*
 * Common header between the regular and MK tuplesort state. We store
 * a flag indicating which one this is, at the beginning, so that
 * the functions that operate on an existing tuplesort are redirected
 * correctly, even if gp_enable_mk_sort changes on the fly.
 */
struct switcheroo_Tuplesortstate
{
	bool		is_mk_tuplesortstate;
};

typedef struct switcheroo_Tuplesortstate switcheroo_Tuplesortstate;

static inline switcheroo_Tuplesortstate *
switcheroo_tuplesort_begin_heap(ScanState *ss, TupleDesc tupDesc,
					 int nkeys, AttrNumber *attNums,
					 Oid *sortOperators, bool *nullsFirstFlags,
					 int workMem, bool randomAccess)
{
	switcheroo_Tuplesortstate *state;

	if (gp_enable_mk_sort)
	{
		state = (switcheroo_Tuplesortstate *)
			tuplesort_begin_heap_mk(ss, tupDesc, nkeys, attNums,
									sortOperators, nullsFirstFlags,
									workMem, randomAccess);
	}
	else
	{
		state = (switcheroo_Tuplesortstate *)
			tuplesort_begin_heap_pg(ss, tupDesc, nkeys, attNums,
									sortOperators, nullsFirstFlags,
									workMem, randomAccess);
	}
	state->is_mk_tuplesortstate = gp_enable_mk_sort;
	return state;
}

static inline switcheroo_Tuplesortstate *
switcheroo_tuplesort_begin_index_btree(Relation indexRel,
							bool enforceUnique,
							int workMem, bool randomAccess)
{
	switcheroo_Tuplesortstate *state;

	if (gp_enable_mk_sort)
	{
		state = (switcheroo_Tuplesortstate *)
			tuplesort_begin_index_mk(indexRel, enforceUnique, workMem, randomAccess);
	}
	else
	{
		state = (switcheroo_Tuplesortstate *)
			tuplesort_begin_index_btree_pg(indexRel, enforceUnique, workMem, randomAccess);
	}
	state->is_mk_tuplesortstate = gp_enable_mk_sort;
	return state;
}
static inline switcheroo_Tuplesortstate *
switcheroo_tuplesort_begin_index_hash(Relation indexRel,
							uint32 hash_mask,
							int workMem, bool randomAccess)
{
	switcheroo_Tuplesortstate *state;

	/* There is no MK variant of this */
	state = (switcheroo_Tuplesortstate *)
		tuplesort_begin_index_hash_pg(indexRel, hash_mask, workMem, randomAccess);
	state->is_mk_tuplesortstate = false;
	return state;
}

static inline switcheroo_Tuplesortstate *
switcheroo_tuplesort_begin_datum(ScanState *ss, Oid datumType,
					  Oid sortOperator, bool nullsFirstFlag,
					  int workMem, bool randomAccess)
{
	switcheroo_Tuplesortstate *state;

	if (gp_enable_mk_sort)
	{
		state = (switcheroo_Tuplesortstate *)
			tuplesort_begin_datum_mk(ss, datumType, sortOperator, nullsFirstFlag, workMem, randomAccess);
	}
	else
	{
		state = (switcheroo_Tuplesortstate *)
			tuplesort_begin_datum_pg(ss, datumType, sortOperator, nullsFirstFlag, workMem, randomAccess);
	}
	state->is_mk_tuplesortstate = gp_enable_mk_sort;
	return state;
}

static inline void
switcheroo_tuplesort_set_bound(switcheroo_Tuplesortstate *state, int64 bound)
{
	if (state->is_mk_tuplesortstate)
		tuplesort_set_bound_mk((Tuplesortstate_mk *) state, bound);
	else
		tuplesort_set_bound_pg((Tuplesortstate_pg *) state, bound);
}

static inline void
switcheroo_tuplesort_puttupleslot(switcheroo_Tuplesortstate *state, TupleTableSlot *slot)
{
	if (state->is_mk_tuplesortstate)
		tuplesort_puttupleslot_mk((Tuplesortstate_mk *) state, slot);
	else
		tuplesort_puttupleslot_pg((Tuplesortstate_pg *) state, slot);
}

static inline void
switcheroo_tuplesort_putindextuple(switcheroo_Tuplesortstate *state, IndexTuple tuple)
{
	if (state->is_mk_tuplesortstate)
		tuplesort_putindextuple_mk((Tuplesortstate_mk *) state, tuple);
	else
		tuplesort_putindextuple_pg((Tuplesortstate_pg *) state, tuple);
}

static inline void
switcheroo_tuplesort_putdatum(switcheroo_Tuplesortstate *state, Datum val, bool isNull)
{
	if (state->is_mk_tuplesortstate)
		tuplesort_putdatum_mk((Tuplesortstate_mk *) state, val, isNull);
	else
		tuplesort_putdatum_pg((Tuplesortstate_pg *) state, val, isNull);
}

static inline void
switcheroo_tuplesort_performsort(switcheroo_Tuplesortstate *state)
{
	if (state->is_mk_tuplesortstate)
		tuplesort_performsort_mk((Tuplesortstate_mk *) state);
	else
		tuplesort_performsort_pg((Tuplesortstate_pg *) state);
}

static inline bool
switcheroo_tuplesort_gettupleslot(switcheroo_Tuplesortstate *state, bool forward,
								   TupleTableSlot *slot)
{
	if (state->is_mk_tuplesortstate)
		return tuplesort_gettupleslot_mk((Tuplesortstate_mk *) state, forward, slot);
	else
		return tuplesort_gettupleslot_pg((Tuplesortstate_pg *) state, forward, slot);
}

static inline IndexTuple
switcheroo_tuplesort_getindextuple(switcheroo_Tuplesortstate *state, bool forward, bool *should_free)
{
	if (state->is_mk_tuplesortstate)
		return tuplesort_getindextuple_mk((Tuplesortstate_mk *) state, forward, should_free);
	else
		return tuplesort_getindextuple_pg((Tuplesortstate_pg *) state, forward, should_free);
}

static inline bool
switcheroo_tuplesort_getdatum(switcheroo_Tuplesortstate *state, bool forward, Datum *val, bool *isNull)
{
	if (state->is_mk_tuplesortstate)
		return tuplesort_getdatum_mk((Tuplesortstate_mk *) state, forward, val, isNull);
	else
		return tuplesort_getdatum_pg((Tuplesortstate_pg *) state, forward, val, isNull);
}

static inline bool
switcheroo_tuplesort_skiptuples(switcheroo_Tuplesortstate *state, int64 ntuples, bool forward)
{
	if (state->is_mk_tuplesortstate)
		return tuplesort_skiptuples_mk((Tuplesortstate_mk *) state, ntuples, forward);
	else
		return tuplesort_skiptuples_pg((Tuplesortstate_pg *) state, ntuples, forward);
}

static inline void
switcheroo_tuplesort_end(switcheroo_Tuplesortstate *state)
{
	if (state->is_mk_tuplesortstate)
		tuplesort_end_mk((Tuplesortstate_mk *) state);
	else
		tuplesort_end_pg((Tuplesortstate_pg *) state);
}

static inline void
switcheroo_tuplesort_rescan(switcheroo_Tuplesortstate *state)
{
	if (state->is_mk_tuplesortstate)
		tuplesort_rescan_mk((Tuplesortstate_mk *) state);
	else
		tuplesort_rescan_pg((Tuplesortstate_pg *) state);
}

static inline void
switcheroo_tuplesort_markpos(switcheroo_Tuplesortstate *state)
{
	if (state->is_mk_tuplesortstate)
		tuplesort_markpos_mk((Tuplesortstate_mk *) state);
	else
		tuplesort_markpos_pg((Tuplesortstate_pg *) state);
}


static inline void
switcheroo_tuplesort_restorepos(switcheroo_Tuplesortstate *state)
{
	if (state->is_mk_tuplesortstate)
		tuplesort_restorepos_mk((Tuplesortstate_mk *) state);
	else
		tuplesort_restorepos_pg((Tuplesortstate_pg *) state);
}

static inline switcheroo_Tuplesortstate *
switcheroo_tuplesort_begin_heap_file_readerwriter(
		ScanState * ss,
		const char* rwfile_prefix, bool isWriter,
		TupleDesc tupDesc,
		int nkeys, AttrNumber *attNums,
		Oid *sortOperators, bool *nullsFirstFlags,
		int workMem, bool randomAccess)
{
	switcheroo_Tuplesortstate *state;

	if (gp_enable_mk_sort)
	{
		state = (switcheroo_Tuplesortstate *)
			tuplesort_begin_heap_file_readerwriter_mk(ss, rwfile_prefix, isWriter,
													  tupDesc, nkeys, attNums,
													  sortOperators, nullsFirstFlags,
													  workMem, randomAccess);
	}
	else
	{
		state = (switcheroo_Tuplesortstate *)
			tuplesort_begin_heap_file_readerwriter_pg(ss, rwfile_prefix, isWriter,
													  tupDesc, nkeys, attNums,
													  sortOperators, nullsFirstFlags,
													  workMem, randomAccess);
	}
	state->is_mk_tuplesortstate = gp_enable_mk_sort;
	return state;
}

static inline void
switcheroo_cdb_tuplesort_init(switcheroo_Tuplesortstate *state, int unique,
							  int sort_flags,
							  int64 maxdistinct)
{
	if (state->is_mk_tuplesortstate)
		cdb_tuplesort_init_mk((Tuplesortstate_mk *) state, unique, sort_flags, maxdistinct);
	else
		cdb_tuplesort_init_pg((Tuplesortstate_pg *) state, unique, sort_flags, maxdistinct);
}

static inline void
switcheroo_tuplesort_begin_pos(switcheroo_Tuplesortstate *state, TuplesortPos **pos)
{
	if (state->is_mk_tuplesortstate)
		tuplesort_begin_pos_mk((Tuplesortstate_mk *) state, (TuplesortPos_mk **) pos);
	else
		tuplesort_begin_pos_pg((Tuplesortstate_pg *) state, pos);
}

static inline bool
switcheroo_tuplesort_gettupleslot_pos(switcheroo_Tuplesortstate *state, TuplesortPos *pos,
                          bool forward, TupleTableSlot *slot, MemoryContext mcontext)
{
	if (state->is_mk_tuplesortstate)
		return tuplesort_gettupleslot_pos_mk((Tuplesortstate_mk *) state, (TuplesortPos_mk *) pos, forward, slot, mcontext);
	else
		return tuplesort_gettupleslot_pos_pg((Tuplesortstate_pg *) state, pos, forward, slot, mcontext);
}

static inline void
switcheroo_tuplesort_flush(switcheroo_Tuplesortstate *state)
{
	if (state->is_mk_tuplesortstate)
		tuplesort_flush_mk((Tuplesortstate_mk *) state);
	else
		tuplesort_flush_pg((Tuplesortstate_pg *) state);
}

static inline void
switcheroo_tuplesort_finalize_stats(switcheroo_Tuplesortstate *state)
{
	if (state->is_mk_tuplesortstate)
		tuplesort_finalize_stats_mk((Tuplesortstate_mk *) state);
	else
		tuplesort_finalize_stats_pg((Tuplesortstate_pg *) state);
}

static inline void
switcheroo_tuplesort_rescan_pos(switcheroo_Tuplesortstate *state, TuplesortPos *pos)
{
	if (state->is_mk_tuplesortstate)
		tuplesort_rescan_pos_mk((Tuplesortstate_mk *) state, (TuplesortPos_mk *) pos);
	else
		tuplesort_rescan_pos_pg((Tuplesortstate_pg *) state, pos);
}

static inline void
switcheroo_tuplesort_markpos_pos(switcheroo_Tuplesortstate *state, TuplesortPos *pos)
{
	if (state->is_mk_tuplesortstate)
		tuplesort_markpos_pos_mk((Tuplesortstate_mk *) state, (TuplesortPos_mk *) pos);
	else
		tuplesort_markpos_pos_pg((Tuplesortstate_pg *) state, pos);
}

static inline void
switcheroo_tuplesort_restorepos_pos(switcheroo_Tuplesortstate *state, TuplesortPos *pos)
{
	if (state->is_mk_tuplesortstate)
		tuplesort_restorepos_pos_mk((Tuplesortstate_mk *) state, (TuplesortPos_mk *) pos);
	else
		tuplesort_restorepos_pos_pg((Tuplesortstate_pg *) state, pos);
}

static inline void
switcheroo_tuplesort_set_instrument(switcheroo_Tuplesortstate *state,
									struct Instrumentation *instrument,
									struct StringInfoData *explainbuf)
{
	if (state->is_mk_tuplesortstate)
		tuplesort_set_instrument_mk((Tuplesortstate_mk *) state, instrument, explainbuf);
	else
		tuplesort_set_instrument_pg((Tuplesortstate_pg *) state, instrument, explainbuf);
}

static inline void
switcheroo_tuplesort_set_gpmon(switcheroo_Tuplesortstate *state,
							   gpmon_packet_t *gpmon_pkt,
							   int *gpmon_tick)
{
	if (state->is_mk_tuplesortstate)
		tuplesort_set_gpmon_mk((Tuplesortstate_mk *) state, gpmon_pkt, gpmon_tick);
	else
		tuplesort_set_gpmon_pg((Tuplesortstate_pg *) state, gpmon_pkt, gpmon_tick);
}

/* these are in tuplesort.h */
#undef Tuplesortstate
#define Tuplesortstate switcheroo_Tuplesortstate

#define tuplesort_begin_heap switcheroo_tuplesort_begin_heap
#define tuplesort_begin_index_btree switcheroo_tuplesort_begin_index_btree
#define tuplesort_begin_index_hash switcheroo_tuplesort_begin_index_hash
#define tuplesort_begin_datum switcheroo_tuplesort_begin_datum
#define tuplesort_set_bound switcheroo_tuplesort_set_bound
#define tuplesort_puttupleslot switcheroo_tuplesort_puttupleslot
#define tuplesort_putindextuple switcheroo_tuplesort_putindextuple
#define tuplesort_putdatum switcheroo_tuplesort_putdatum
#define tuplesort_performsort switcheroo_tuplesort_performsort
#define tuplesort_gettupleslot switcheroo_tuplesort_gettupleslot
#define tuplesort_getindextuple switcheroo_tuplesort_getindextuple
#define tuplesort_getdatum switcheroo_tuplesort_getdatum
#define tuplesort_skiptuples switcheroo_tuplesort_skiptuples
#define tuplesort_end switcheroo_tuplesort_end
#define tuplesort_rescan switcheroo_tuplesort_rescan
#define tuplesort_markpos switcheroo_tuplesort_markpos
#define tuplesort_restorepos switcheroo_tuplesort_restorepos

/* these are in tuplesort_gp.h */
#define tuplesort_begin_heap_file_readerwriter switcheroo_tuplesort_begin_heap_file_readerwriter
#define cdb_tuplesort_init switcheroo_cdb_tuplesort_init
#define tuplesort_begin_pos switcheroo_tuplesort_begin_pos
#define tuplesort_gettupleslot_pos switcheroo_tuplesort_gettupleslot_pos
#define tuplesort_flush switcheroo_tuplesort_flush
#define tuplesort_finalize_stats switcheroo_tuplesort_finalize_stats
#define tuplesort_rescan_pos switcheroo_tuplesort_rescan_pos
#define tuplesort_markpos_pos switcheroo_tuplesort_markpos_pos
#define tuplesort_restorepos_pos switcheroo_tuplesort_restorepos_pos
#define tuplesort_set_instrument switcheroo_tuplesort_set_instrument
#define tuplesort_set_gpmon switcheroo_tuplesort_set_gpmon

#endif


#endif   /* TUPLESORT_H */

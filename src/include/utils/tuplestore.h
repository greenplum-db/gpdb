/*-------------------------------------------------------------------------
 *
 * tuplestore.h
 *	  Generalized routines for temporary tuple storage.
 *
 * This module handles temporary storage of tuples for purposes such
 * as Materialize nodes, hashjoin batch files, etc.  It is essentially
 * a dumbed-down version of tuplesort.c; it does no sorting of tuples
 * but can only store and regurgitate a sequence of tuples.  However,
 * because no sort is required, it is allowed to start reading the sequence
 * before it has all been written.	This is particularly useful for cursors,
 * because it allows random access within the already-scanned portion of
 * a query without having to process the underlying scan to completion.
 * A temporary file is used to handle the data if it exceeds the
 * space limit specified by the caller.
 *
 * Beginning in Postgres 8.2, what is stored is just MinimalTuples;
 * callers cannot expect valid system columns in regurgitated tuples.
 * Also, we have changed the API to return tuples in TupleTableSlots,
 * so that there is a check to prevent attempted access to system columns.
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/utils/tuplestore.h,v 1.22 2008/01/01 19:45:59 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef TUPLESTORE_H
#define TUPLESTORE_H

#include "executor/tuptable.h"

struct Instrumentation;                 /* #include "executor/instrument.h" */


/* Tuplestorestate and tuplestorepos are opaque types whose details are not known outside
 * tuplestore.c.
 */
typedef struct Tuplestorestate Tuplestorestate;
typedef struct TuplestorePos TuplestorePos;

/*
 * tuplestore_set_instrument
 *
 * May be called after tuplestore_begin_xxx() to enable reporting of
 * statistics and events for EXPLAIN ANALYZE.
 *
 * The 'instr' ptr is retained in the 'state' object.  The caller must
 * ensure that it remains valid for the life of the Tuplestorestate object.
 */
void 
tuplestore_set_instrument(Tuplestorestate          *state,
                          struct Instrumentation   *instrument);

/*
 * Currently we only need to store MinimalTuples, but it would be easy
 * to support the same behavior for IndexTuples and/or bare Datums.
 */

extern void tuplestore_begin_pos(Tuplestorestate* state, TuplestorePos **pos);
extern Tuplestorestate* tuplestore_begin_heap(bool randomAccess,
					  bool interXact,
					  int maxKBytes);

extern void tuplestore_set_eflags(Tuplestorestate *state, int eflags);

extern void tuplestore_puttupleslot_pos(Tuplestorestate *state, TuplestorePos *pos, TupleTableSlot *slot);
extern void tuplestore_puttupleslot(Tuplestorestate *state, TupleTableSlot *slot);

extern void tuplestore_puttuple_pos(Tuplestorestate *state, TuplestorePos *pos, HeapTuple tuple);

extern void tuplestore_puttuple(Tuplestorestate *state, HeapTuple tuple);

/* tuplestore_donestoring() used to be required, but is no longer used */
#define tuplestore_donestoring(state)	((void) 0)

extern bool tuplestore_gettupleslot_pos(Tuplestorestate *state, TuplestorePos *pos, bool forward,
						TupleTableSlot *slot);
extern bool tuplestore_gettupleslot(Tuplestorestate *state, bool forward, TupleTableSlot *slot);

extern bool tuplestore_advance_pos(Tuplestorestate *state, TuplestorePos *pos, bool forward);
extern bool tuplestore_advance(Tuplestorestate *state, bool forward);

extern void tuplestore_end(Tuplestorestate *state);

extern bool tuplestore_ateof_pos(Tuplestorestate *state, TuplestorePos *pos);
extern bool tuplestore_ateof(Tuplestorestate *state);

extern void tuplestore_rescan_pos(Tuplestorestate *state, TuplestorePos *pos);
extern void tuplestore_rescan(Tuplestorestate *state);

extern void tuplestore_markpos_pos(Tuplestorestate *state, TuplestorePos *pos);
extern void tuplestore_markpos(Tuplestorestate *state);

extern void tuplestore_restorepos_pos(Tuplestorestate *state, TuplestorePos *pos);
extern void tuplestore_restorepos(Tuplestorestate *state);

extern void tuplestore_flush(Tuplestorestate *state);

#endif   /* TUPLESTORE_H */

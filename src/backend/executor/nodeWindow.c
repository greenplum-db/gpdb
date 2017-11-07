/*-------------------------------------------------------------------------
 *
 * nodeWindowNew.c
 *	  Routines to handle window nodes.
 *
 *
 * Portions Copyright (c) 2007-2012, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeWindow.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

/* XXX include list is speculative -- bhagenbuch */
#include "access/heapam.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "catalog/pg_window.h"
#include "cdb/cdbvars.h"
#include "executor/executor.h"
#include "executor/nodeAgg.h"
#include "executor/nodeWindow.h"
#include "optimizer/clauses.h"
#include "nodes/makefuncs.h"
#include "parser/parse_agg.h"
#include "parser/parse_coerce.h"
#include "parser/parse_expr.h"
#include "parser/parse_oper.h"
#include "utils/acl.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/numeric.h"
#include "utils/syscache.h"
#include "utils/tuplestorenew.h"

/* Check for aggregate functions that have only transition functions,
 * but not inverse preliminary functions or preliminary functions.
 */
#define HAS_ONLY_TRANS_FUNC(funcstate) \
	(funcstate->plain_agg && \
	 !OidIsValid(funcstate->invprelimfn_oid) && \
	 !OidIsValid(funcstate->prelimfn_oid) && \
	 OidIsValid(funcstate->transfn_oid))

/*
 * A struct to hold all the data relevant to a value, used for
 * buffering.
 */
typedef struct WindowValue
{
	/*
	 * Total number of not NULL argument for the function associated with this
	 * value.
	 *
	 * This is only used for aggregate functions that have inverse preliminary
	 * functions. For this kind of functions, when their argument values are
	 * all NULLs in the current frame, their result value should be NULL as
	 * well. This value is used to tell if all argument values in the current
	 * frame are all NULLs.
	 */
	uint64		numNotNulls;
	Datum		value;
	bool		valueIsNull;
}	WindowValue;

/*
 * A higher-level structure to represent an entry in the frame buffer.
 */
typedef struct FrameBufferEntry
{
	/*
	 * Keys for RANGE frames.
	 *
	 * This is only used for RANGE frames. For ROWS frame, these values are NULL.
	 */
	Datum	   *keys;
	bool	   *nulls;

	/*
	 * A list of intermediate function values to be buffered in the frame
	 * buffer. Each of values is of WindowValue type.
	 */
	List	   *func_values;
} FrameBufferEntry;

/*
 * WindowStatePerLevelData - per-level working state
 */
typedef struct WindowStatePerLevelData
{
	FmgrInfo   *eqfunctions;	/* equality fns for partial key */
	FmgrInfo   *ltfunctions;	/* less-than functions for partial key */
	bool		need_peercount;

	int64		group_index;
	int64		prior_non_peer_count;
	int64		peer_index;
	int64		peer_count;
	int64		rank;
	int64		dense_rank;
	int64		prior_rank;
	int64		prior_dense_rank;

	/* List of functions in this key level */
	List	   *level_funcs;

	/* The framing clause */
	int			frameOptions;
	Node	   *startOffset;
	Node	   *endOffset;

	/* Indicate if all functions have trivial frames. */
	bool		trivial_frames_only;

	/*
	 * Indicate if the frame clause for this level has an edge of DELAY_BOUND
	 * type.
	 */
	bool		has_delay_trail;
	bool		has_delay_lead;
	bool		has_delay_bound; // OR of the above

	/*
	 * Indicate if all functions in this level are aggregate functions that
	 * have no preliminary functions or inverse preliminary functions.
	 */
	bool		has_only_trans_funcs;

	/* user can specify ascending or descending order, record it here */
	bool	   *col_sort_asc;

	/*
	 * The partial order by keys. There is at least one sort key in this
	 * array. It is used for convenience to find the order by keys since we
	 * need to store them in the frame buffer for RANGE-based framing.
	 */
	int			numSortCols;
	AttrNumber *sortColIdx;
	Oid		   *sortOperators;

	/* column type data */
	Oid		   *col_types;
	int2	   *col_typlens;
	bool	   *col_typbyvals;

	/*
	 * Frame is contradictory and no tuples will fall into the frame, for
	 * example: BETWEEN 10 FOLLOWING AND 10 PRECEDING.
	 */
	bool		empty_frame;

	/* state for rows frame is simple (for now) */
	int64		trail_rows;
	int64		lead_rows;

	/* state for range frame is more complex */
	Datum		trail_range;
	Datum		lead_range;

	/*
	 * The number of rows between the trailing edge and the current_row, and
	 * between the leading edge and the current_row.
	 *
	 * For preceding edges, these numbers are 0 or negative.
	 */
	int64		num_trail_rows;
	int64		num_lead_rows;

	/* state for frame edges (for both ROWS and RANGE frames) */
	ExprState  *trail_expr;
	ExprState  *lead_expr;

	/* state for RANGE-frame edge computation */
	ExprState  *trail_range_expr;
	ExprState  *lead_range_expr;

	/* RANGE frame only; see comments on init_bound_frame_edge_expr */
	ExprState  *trail_range_eq_expr;
	ExprState  *lead_range_eq_expr;

	/*
	 * The frame buffer to buffer the intermediate results for the functions
	 * in this level key.
	 */
	struct WindowFrameBufferData *frame_buffer;

	/*
	 * These two readers are pointing to the trailing and leading edges of
	 * this frame, respectively.
	 */
	NTupleStoreAccessor *trail_reader;
	NTupleStoreAccessor *lead_reader;

	/*
	 * Indicate if lead_reader is pointing to the exact edge position. With
	 * this flag, we don't need to do the tuple comparison again during
	 * checking if there are sufficient data to generate an output.
	 */
	bool		lead_ready;

	/*
	 * Indicate if funcstate->aggTransValue actually contains the aggregate
	 * value.
	 */
	bool		agg_filled;

	/*
	 * FrameBufferEntry buffers to avoid pallocs/pfrees.
	 */
	FrameBufferEntry *curr_entry_buf;
	FrameBufferEntry *trail_entry_buf;
	FrameBufferEntry *lead_entry_buf;
	
	/* A char buffer to temporarily hold serialized data before
	*  writing them to the frame buffer, and keep deserialized
	*  data when reading from the frame buffer.
	*
	* Use this pre-allocated buffer to avoid doing
	* palloc/pfree many times.
	*/
	StringInfoData serial_buf;
}	WindowStatePerLevelData;

/*
 * WindowStatePerFunctionData
 */
typedef struct WindowStatePerFunctionData
{
	/* Links to WindowFunc expr and state nodes this working state is for */
	WindowFuncExprState *wfuncstate;
	WindowFunc *wfunc;
	bool		allowframe;

	/* Indicate if this function requires a range cumulative frame. */
	bool		cumul_frame;

	bool		winpeercount;

	int			numargs;		/* explicit, caller-supplied args */
	int16		resulttypeLen;
	bool		resulttypeByVal;

	bool		plain_agg;		/* is it just a plain aggregate function? */

	/* Ordinary window function */
	Datum		win_value;
	bool		win_value_is_null;
	FmgrInfo	flinfo;

	/* Aggregate-derived window function */
	Oid			transfn_oid;
	Oid			finalfn_oid;
	Oid			prelimfn_oid;
	Oid			invtransfn_oid;
	Oid			invprelimfn_oid;

	FmgrInfo	transfn;
	FmgrInfo	finalfn;
	FmgrInfo	prelimfn;
	FmgrInfo	invtransfn;
	FmgrInfo	invprelimfn;

	Datum		aggInitValue;
	bool		aggInitValueIsNull;

	int16		aggTranstypeLen;
	bool		aggTranstypeByVal;

	/* the intermediate transition value */
	Datum		aggTransValue;
	bool		aggTransValueIsNull;
	bool		aggNoTransValue;

	/* the final transition value for output tuples */
	Datum		final_aggTransValue;
	bool		final_aggTransValueIsNull;
	bool		final_aggNoTransValue;

	/*
	 * The final transition value may be a copy of a value stored in the frame
	 * buffre, or the above intermediate in-memory transition value. We should
	 * free the copied value. This boolean is used to determine if
	 * final_aggTransValue should be freed.
	 */
	bool		final_aggShouldFree;

	/* frame does not require buffering and complexity of invokeWindowFuncs() */
	bool		trivial_frame;

	/*
	 * The index for the intermediate value of this function when serializing
	 * its value along with others before storing them into frame buffers.
	 *
	 * The initial value is -1.
	 */
	int			serial_index;

	/*
	 * The total number of not NULL arguments for this function so far.
	 */
	uint64		numNotNulls;
}	WindowStatePerFunctionData;

typedef enum
{
	EDGE_TRAIL,
	EDGE_LEAD
} WindowEdge;

#define EDGE_TRAIL_IS_BOUND(level_state) \
	(((level_state)->frameOptions & FRAMEOPTION_START_VALUE) != 0)
#define EDGE_LEAD_IS_BOUND(level_state) \
	(((level_state)->frameOptions & FRAMEOPTION_END_VALUE) != 0)
#define EDGE_IS_BOUND(level_state, edge) \
	(((edge) == EDGE_LEAD) ? EDGE_LEAD_IS_BOUND(level_state) : EDGE_TRAIL_IS_BOUND(level_state))

#define EDGE_TRAIL_IS_BOUND_FOLLOWING(level_state) \
	(((level_state)->frameOptions & FRAMEOPTION_START_VALUE_FOLLOWING) != 0)
#define EDGE_LEAD_IS_BOUND_FOLLOWING(level_state) \
	(((level_state)->frameOptions & FRAMEOPTION_END_VALUE_FOLLOWING) != 0)
#define EDGE_TRAIL_IS_BOUND_PRECEDING(level_state) \
	(((level_state)->frameOptions & FRAMEOPTION_START_VALUE_PRECEDING) != 0)
#define EDGE_LEAD_IS_BOUND_PRECEDING(level_state) \
	(((level_state)->frameOptions & FRAMEOPTION_END_VALUE_PRECEDING) != 0)

#define EDGE_IS_BOUND_FOLLOWING(level_state, edge) \
	((edge == EDGE_LEAD) ? EDGE_LEAD_IS_BOUND_FOLLOWING(level_state) : \
	 EDGE_TRAIL_IS_BOUND_FOLLOWING(level_state))
#define EDGE_IS_BOUND_PRECEDING(level_state, edge) \
	((edge == EDGE_LEAD) ? EDGE_LEAD_IS_BOUND_PRECEDING(level_state) : \
	 EDGE_TRAIL_IS_BOUND_PRECEDING(level_state))
#define EDGE_IS_BOUND(level_state, edge) \
	(((edge) == EDGE_LEAD) ? EDGE_LEAD_IS_BOUND(level_state) : \
	 EDGE_TRAIL_IS_BOUND(level_state))
#define EDGE_EQ_CURRENT_ROW(level_state, wstate, edge) \
	((edge == EDGE_LEAD) ? \
	 EDGE_LEAD_EQ_CURRENT_ROW(level_state, wstate) : \
	 EDGE_TRAIL_EQ_CURRENT_ROW(level_state, wstate))

#define EDGE_IS_ROWS(level_state) \
	(((level_state)->frameOptions & FRAMEOPTION_ROWS) != 0)

static bool exec_eq_exprstate(WindowState * wstate, ExprState *eq_exprstate);

/* These are too complicated to be macros. */
static bool
EDGE_TRAIL_EQ_CURRENT_ROW(WindowStatePerLevel level_state,
						  WindowState *wstate)
{
	if ((level_state->frameOptions & FRAMEOPTION_START_CURRENT_ROW) != 0)
		return true;

	if (EDGE_TRAIL_IS_BOUND(level_state))
	{
		if (EDGE_IS_ROWS(level_state))
			return (level_state->trail_rows == 0);
		else
			return exec_eq_exprstate(wstate, level_state->trail_range_eq_expr);
	}
	else
		return false;
}

static bool
EDGE_LEAD_EQ_CURRENT_ROW(WindowStatePerLevel level_state,
						 WindowState *wstate)
{
	if ((level_state->frameOptions & FRAMEOPTION_END_CURRENT_ROW) != 0)
		return true;

	if (EDGE_LEAD_IS_BOUND(level_state))
	{
		if (EDGE_IS_ROWS(level_state))
			return (level_state->lead_rows == 0);
		else
			return exec_eq_exprstate(wstate, level_state->lead_range_eq_expr);
	}
	else
		return false;
}


/****************************************************
 * Window Input Buffer and its APIs
 ***************************************************/
/*
 * WindowInputBuffer: this buffer holds input tuples that fit in the
 * partition which is currently processed by the window node. This
 * buffer also stores the first tuple in the next partition, which
 * is always the last tuple in the buffer.
 */
typedef struct WindowInputBufferData
{
	NTupleStore *tuplestore;
	NTupleStoreAccessor *writer;

	/* The reader that is always pointed to the previous current_row */
	NTupleStoreAccessor *current_row_reader;

	/* The reader that reads tuples in the buffer */
	NTupleStoreAccessor *reader;

	/*
	 * Indicate if the last tuple in the buffer is the tuple that breaks the
	 * partition key.
	 */
	bool		part_break;

	/* The total number of tuples in the store. */
	int64		num_tuples;
}	WindowInputBufferData;

static void resetInputBuffer(WindowState * wstate);
static void freeInputBuffer(WindowState * wstate);
static void trimInputBuffer(WindowInputBuffer buffer);

/********************************************************
 * Window Frame Buffer and its APIs
 *******************************************************/
/*
 * WindowFrameBuffer: this buffer holds intermediate aggregate values
 * or input arguments in order to generate the final aggregates for
 * the next output row.
 *
 * This buffer will trim old values when they are outside the lower bound
 * of the given frame provided to this buffer. This frame can be a RANGE
 * frame or a ROW frame. Note that if this frame is a RANGE frame, the
 * actual number of values in the buffer may vary over time.
 *
 * This buffer is designed to be one for each key level.  We could
 * also share one buffer among key levels if they have the same list
 * functions.
 */
typedef struct WindowFrameBufferData
{
	NTupleStore *tuplestore;
	NTupleStoreAccessor *writer;

	/*
	 * This reader is used to scan through the buffer to compute the final
	 * result.
	 */
	NTupleStoreAccessor *reader;

	/* Indicate if this buffer is for a RANGE frame or a ROWS frame. */
	bool		is_rows;

	/*
	 * The number of rows before and after the current_row. Note that
	 * 'num_rows_after' counts the current_row.
	 *
	 * These two counters are used to adjust frame edges for frames with
	 * expressions.
	 *
	 * Currently, these two counters do not be modified after a trim operation.
	 */
	int64		num_rows_before;
	int64		num_rows_after;

	/*
	 * The trailing and leading number of rows from current_row if this frame
	 * is a ROW frame.
	 */
	int64		trail_rows;
	int64		lead_rows;

	/*
	 * The trailing and leading range from current_row if this frame is a
	 * RANGE frame.
	 */
	Datum		trail_range;
	Datum		lead_range;

	/* The reader that always points to the current_row. */
	NTupleStoreAccessor *current_row_reader;

	/*
	 * The accessor that defines the point before which all values in the
	 * buffer can be trimmed.
	 */
	NTupleStoreAccessor *trim_reader;

	/*
	 * Pointer to the level state. Information about window functions can be
	 * found here.
	 */
	WindowStatePerLevel level_state;
}	WindowFrameBufferData;
typedef WindowFrameBufferData *WindowFrameBuffer;

static WindowFrameBuffer createRangeFrameBuffer(Datum trail_range,
					   Datum lead_range,
					   int64 bytes);
static WindowFrameBuffer createRowsFrameBuffer(int64 trail_rows,
					  int64 lead_rows,
					  int64 bytes);
static WindowFrameBuffer resetFrameBuffer(WindowFrameBuffer buffer);
static void appendToFrameBuffer(WindowStatePerLevel level_state,
					WindowState * wstate,
					bool last_peer);
static void trimFrameBuffer(WindowFrameBuffer buffer);
static void incrementCurrentRow(WindowFrameBuffer buffer, WindowState * wstate);
static bool hasEnoughDataInRange(WindowFrameBuffer buffer,
					 WindowStatePerLevel level_state,
					 WindowState * wstate,
					 Datum tail_range,
					 Datum lead_range);
static bool hasEnoughDataInRows(WindowFrameBuffer buffer,
					WindowStatePerLevel level_state,
					WindowState * wstate,
					int64 trail_rows,
					int64 lead_rows);
static void computeFrameValue(WindowStatePerLevel level_state,
				  WindowState * wstate,
				  NTupleStoreAccessor * trail_reader,
				  NTupleStoreAccessor * lead_reader);

static void createFrameBuffers(WindowState * wstate);
static void resetFrameBuffers(WindowState * wstate);
static void resetTransValues(WindowStatePerLevel level_state,
				 WindowState * wstate);
static void freeFrameBuffer(WindowFrameBuffer buffer);
static void freeFrameBuffers(WindowState * wstate);

/*
 * WindowBufferCursor
 * This is an abstract cursor to scan the frame buffer.  This holds transient
 * state of a window level, so you have to make sure the scan on the same window
 * level should happen once at a time.
 */
typedef struct WindowBufferCursorData
{
	WindowState *wstate;		/* state of this window */
	WindowStatePerLevel level_state;	/* state of this window level */
	NTupleStoreAccessor *reader;	/* reader to scan rows */
	NTupleStorePos orig_pos;	/* original position of the reader */
	bool		use_last_agg;	/* should scan "last_agg"? */
	bool		reader_eof;		/* is reader at end of scan? */
	bool		has_tuples;		/* does frame has tuples? */
	bool		forward;		/* scan direction */
	bool		first;			/* is first fetch of tupstore? */
}	WindowBufferCursorData;

typedef struct WindowBufferCursorData *WindowBufferCursor;

static WindowBufferCursor windowBufferBeginScan(WindowState * wstate,
					  WindowStatePerLevel level_state, bool forward);
static List *windowBufferNextTupleStore(WindowBufferCursor cursor);
static List *windowBufferNextLastAgg(WindowBufferCursor cursor);
static List *windowBufferNext(WindowBufferCursor cursor);
static void windowBufferEndScan(WindowBufferCursor cursor);


/* the functions for the Window node */
static void initialize_peragg(WindowState *winstate, WindowFunc *wfunc,
							  WindowStatePerFunction funcstate);

static WindowState *makeWindowState(WindowAgg *window, EState *estate);
static void initializePartition(WindowState * wstate);
static bool checkOutputReady(WindowState * wstate);
static TupleTableSlot *fetchTupleSlotThroughBuf(WindowState * wstate);
static int initFcinfo(WindowFuncExprState *wfxstate, FunctionCallInfoData *fcinfo,
		   WindowStatePerFunction funcstate, ExprContext *econtext,
		   bool check_nulls);
static void adjustEdges(WindowFrameBuffer buffer, WindowState * wstate);
static TupleTableSlot *fetchCurrentRow(WindowState * wstate);
static TupleTableSlot *fetchTupleSlotThroughBuf(WindowState * wstate);
static void processTupleSlot(WindowState * wstate, TupleTableSlot * slot, bool last_peer);
static bool getCurrentValue(NTupleStoreAccessor * reader,
				WindowStatePerLevel level_state,
				FrameBufferEntry *entry_buf);
static bool cmp_deformed_tuple(Datum *a, bool *a_nulls, Datum *b, bool *b_nulls,
				   int ncols,
				   FmgrInfo *ltfuncs, FmgrInfo *eqfuncs,
				   MemoryContext evalContext, bool is_equal);
static FmgrInfo *get_ltfuncs(int numCols, Oid *ltOperators);
static void advanceKeyLevelState(WindowState *wstate);
static void init_frames(WindowState * wstate);
static void deform_window_tuple(TupleTableSlot * slot, int nattrs, AttrNumber *attnums,
					Datum *values, bool *nulls, WindowState * wstate);
static void add_tuple_to_trans(WindowStatePerFunction funcstate, WindowState * wstate,
				   ExprContext *econtext, bool check_nulls);
static bool hasTuplesInFrame(WindowStatePerLevel level_state,
				 WindowState * wstate);
static Datum last_value_internal(WindowFuncExprState *wfxstate, bool *isnull);
static Datum first_value_internal(WindowFuncExprState *wfxstate, bool *isnull);
static FrameBufferEntry *createFrameBufferEntry(WindowStatePerLevel level_state);
static void freeFrameBufferEntry(FrameBufferEntry *entry);
static void advanceEdgeForRange(WindowStatePerLevel level_state,
					WindowState * wstate,
					ExprState *edge_expr,
					ExprState *edge_range_expr,
					NTupleStoreAccessor * edge_reader,
					WindowEdge edge);
static void forwardEdgeForRange(WindowStatePerLevel level_state,
					WindowState *wstate,
					ExprState *edge_expr,
					ExprState *edge_range_expr,
					NTupleStoreAccessor * edge_reader,
					WindowEdge edge);
static bool checkLastRowForEdge(WindowStatePerLevel level_state,
					WindowState * wstate,
					NTupleStoreAccessor * edge_reader,
					Datum new_edge_value,
					bool new_edge_value_isnull,
					WindowEdge edge);
static ExprState *make_eq_exprstate(WindowState * wstate,
				  Expr *expr1, Expr *expr2);
static void setEmptyFrame(WindowStatePerLevel level_state,
			  WindowState * wstate);

/*
 * initFrameBuffer -- initialize the frame buffer.
 *
 * Create the tuplestore and all accessors.
 */
static void
initFrameBuffer(WindowFrameBuffer buffer, int64 bytes)
{
	buffer->tuplestore = ntuplestore_create(bytes);

	/* Create accessors. */
	buffer->writer = ntuplestore_create_accessor(buffer->tuplestore, true);
	buffer->reader = ntuplestore_create_accessor(buffer->tuplestore, false);

	buffer->current_row_reader =
		ntuplestore_create_accessor(buffer->tuplestore, false);
	buffer->trim_reader =
		ntuplestore_create_accessor(buffer->tuplestore, false);

	buffer->num_rows_before = buffer->num_rows_after = 0;
}

/*
 * createRangeFrameBuffer -- create a new WindowFrameBuffer of the RANGE type.
 */
static WindowFrameBuffer
createRangeFrameBuffer(Datum trail_range, Datum lead_range, int64 bytes)
{
	WindowFrameBuffer buffer =
	(WindowFrameBuffer) palloc0(sizeof(WindowFrameBufferData));

	buffer->is_rows = false;
	buffer->trail_range = trail_range;
	buffer->lead_range = lead_range;

	initFrameBuffer(buffer, bytes);

	return buffer;
}

/*
 * createRowsFrameBuffer -- create a new WindowFrameBuffer of the ROWS type.
 */
static WindowFrameBuffer
createRowsFrameBuffer(int64 trail_rows, int64 lead_rows, int64 bytes)
{
	WindowFrameBuffer buffer =
	(WindowFrameBuffer) palloc0(sizeof(WindowFrameBufferData));

	buffer->is_rows = true;
	buffer->trail_rows = trail_rows;
	buffer->lead_rows = lead_rows;

	initFrameBuffer(buffer, bytes);

	return buffer;
}

/*
 * createFrameBuffers -- create frame buffers for all key levels inside
 * a given WindowState.
 */
static void
createFrameBuffers(WindowState *wstate)
{
	int64		bytes;

	if (wstate->trivial_frame)
		return;

	bytes = ((PlanStateOperatorMemKB((PlanState *) wstate) * 1024L) / 2);

	WindowStatePerLevel level_state = wstate->level_state;

	Assert(level_state->frame_buffer == NULL);
	if (EDGE_IS_ROWS(level_state))
		level_state->frame_buffer =
			createRowsFrameBuffer(level_state->trail_rows,
								  level_state->lead_rows,
								  bytes);
	else
		level_state->frame_buffer =
			createRangeFrameBuffer(level_state->trail_range,
								   level_state->lead_range,
								   bytes);

	level_state->trail_reader =
		ntuplestore_create_accessor(level_state->frame_buffer->tuplestore, false);
	level_state->lead_reader =
		ntuplestore_create_accessor(level_state->frame_buffer->tuplestore, false);

	level_state->frame_buffer->level_state = level_state;
}

/*
 * resetFrameBuffer -- reset or initialize a window frame buffer.
 *
 * Reset the tuplestore in the buffer, and re-create all accessors.
 *
 * The input argument 'buffer' can not be NULL.
 */
static WindowFrameBuffer
resetFrameBuffer(WindowFrameBuffer buffer)
{
	Assert(buffer != NULL);

	/* destroy all accessors */
	ntuplestore_destroy_accessor(buffer->writer);
	ntuplestore_destroy_accessor(buffer->reader);
	ntuplestore_destroy_accessor(buffer->current_row_reader);
	ntuplestore_destroy_accessor(buffer->trim_reader);

	ntuplestore_reset(buffer->tuplestore);

	/* (Re)create accessors. */
	buffer->writer = ntuplestore_create_accessor(buffer->tuplestore, true);
	buffer->reader = ntuplestore_create_accessor(buffer->tuplestore, false);

	buffer->current_row_reader =
		ntuplestore_create_accessor(buffer->tuplestore, false);
	buffer->trim_reader =
		ntuplestore_create_accessor(buffer->tuplestore, false);

	buffer->num_rows_before = buffer->num_rows_after = 0;

	return buffer;
}

/*
 * resetFrameBuffers -- reset all frame buffers in a WindowState.
 */
static void
resetFrameBuffers(WindowState *wstate)
{
	if (!wstate->trivial_frame)
	{
		WindowStatePerLevel level_state = wstate->level_state;

		if (level_state->frame_buffer)
		{
			/* destroy its accessors */
			if (level_state->trail_reader)
				ntuplestore_destroy_accessor(level_state->trail_reader);
			if (level_state->lead_reader)
				ntuplestore_destroy_accessor(level_state->lead_reader);

			level_state->frame_buffer = resetFrameBuffer(level_state->frame_buffer);

			/* (Re)create accessors */
			level_state->trail_reader =
				ntuplestore_create_accessor(level_state->frame_buffer->tuplestore, false);
			level_state->lead_reader =
				ntuplestore_create_accessor(level_state->frame_buffer->tuplestore, false);
		}

		level_state->num_trail_rows = 0;
		level_state->num_lead_rows = 0;
		level_state->lead_ready = false;
	}
}

/*
 * freeFrameBuffer -- release the space for a given WindowFrameBuffer.
 */
static void
freeFrameBuffer(WindowFrameBuffer buffer)
{
	if (buffer == NULL)
		return;
	ntuplestore_destroy_accessor(buffer->writer);
	ntuplestore_destroy_accessor(buffer->reader);
	ntuplestore_destroy_accessor(buffer->current_row_reader);

	ntuplestore_destroy(buffer->tuplestore);

	pfree(buffer);
}

/*
 * freeFrameBuffers -- release the space for all frame buffers.
 */
static void
freeFrameBuffers(WindowState *wstate)
{
	if (!wstate->trivial_frame)
	{
		WindowStatePerLevel level_state = wstate->level_state;

		if (level_state->frame_buffer)
		{
			/* destroy its accessors */
			if (level_state->trail_reader)
				ntuplestore_destroy_accessor(level_state->trail_reader);
			if (level_state->lead_reader)
				ntuplestore_destroy_accessor(level_state->lead_reader);

			freeFrameBuffer(level_state->frame_buffer);
			level_state->frame_buffer = NULL;
		}

	}
}

/*
 * serializeValue -- serialize a given value (Datum) into a StringInfo
 */
static void
serializeValue(Datum value, bool isnull,
			   bool byvalue, int16 typelen,
			   StringInfo serial_buf)
{
	/* isnull col */
	appendStringInfoChar(serial_buf, isnull);

	if (!isnull)
	{
		if (byvalue)
			appendBinaryStringInfo(serial_buf, &value, sizeof(Datum));
		else
		{
			Size		real_size = datumGetSize(value, byvalue, typelen);

			/*
			 * The pointer needs to be properly aligned since the deserializer
			 * will not make a copy for this Datum, but simply convert this
			 * pointer address to a Datum.
			 */
			Size		alignmentBytes = MAXALIGN(serial_buf->len) - serial_buf->len;

			appendStringInfoFill(serial_buf, alignmentBytes, '\0');

			appendBinaryStringInfo(serial_buf,
								   DatumGetPointer(value),
								   real_size);
		}
	}
}

/*
 * serializeFuncs -- serialize values for all functions in a given
 * level and put it into a given space, starting from 'start_pos'.
 */
static void
serializeFuncs(WindowStatePerLevel level_state,
			   ExprContext *econtext,
			   StringInfo serial_buf)
{
	ListCell   *lc;
	int			serial_index = -1;

	/* the value for each function. */
	foreach(lc, level_state->level_funcs)
	{
		WindowStatePerFunction funcstate = (WindowStatePerFunction) lfirst(lc);

		if (funcstate->trivial_frame)
		{
			/* ignore trivial window functions */
			continue;
		}
		else if (funcstate->winpeercount)
		{
			/* functions that require peer counts. */
			int16		typelen;
			bool		byvalue;

			get_typlenbyval(CUME_DIST_PRELIM_TYPE,
							&typelen, &byvalue);

			serializeValue(funcstate->win_value,
						   funcstate->win_value_is_null,
						   byvalue, typelen,
						   serial_buf);
		}
		else if (IS_LEAD_LAG(funcstate->wfuncstate->winkind) ||
				 IS_FIRST_LAST(funcstate->wfuncstate->winkind))
		{
			WindowFuncExprState *wfxstate;
			int			nargs;
			int			argno = 1;

			wfxstate = funcstate->wfuncstate;
			nargs = list_length(wfxstate->args);
			Assert(2 <= nargs && nargs <= 4);

			serializeValue(funcstate->aggTransValue,
						   funcstate->aggTransValueIsNull,
						   wfxstate->argtypbyval[argno],
						   wfxstate->argtyplen[argno],
						   serial_buf);
		}

		/*
		 * the function which has an inverse preliminary function or
		 * preliminary function.
		 */
		else if (OidIsValid(funcstate->invprelimfn_oid) ||
				 OidIsValid(funcstate->prelimfn_oid))
		{
			/*
			 * Store number of not NULLS when the function has a inverse
			 * preliminary function.
			 */
			if (OidIsValid(funcstate->invprelimfn_oid))
				appendBinaryStringInfo(serial_buf, &funcstate->numNotNulls, sizeof(uint64));

			serializeValue(funcstate->aggTransValue,
						   funcstate->aggTransValueIsNull,
						   funcstate->aggTranstypeByVal,
						   funcstate->aggTranstypeLen,
						   serial_buf);
		}
		else
		{
			/* the general case */
			WindowFuncExprState *winref_state = funcstate->wfuncstate;
			ListCell   *ref_lc;
			Oid			typid;
			bool		byval;
			int16		arglen;

			/* Store the input arguments */
			foreach(ref_lc, winref_state->args)
			{
				ExprState  *argstate = (ExprState *)lfirst(ref_lc);
				Datum		value;
				bool		isnull;
				MemoryContext oldctx;

				typid = exprType((Node *) argstate->expr);
				get_typlenbyval(typid, &arglen, &byval);

				oldctx = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);
				value = ExecEvalExpr(argstate, econtext, &isnull, NULL);
				MemoryContextSwitchTo(oldctx);

				serializeValue(value, isnull,
							   byval, arglen,
							   serial_buf);
			}
		}

		serial_index++;
		funcstate->serial_index = serial_index;
	}
}

/*
 * deserializeValue -- deserialize a Datum value from a given char array.
 *
 * This function reads the char array as specified by 'read_pos', and
 * deserialize one serialized Datum values in this array into a given
 * WindowValue. If a Datum is passed by reference, this function does _not_
 * make a copy of this Datum, but simply set the pointer to the
 * corresponding address of the given char array.
 *
 * The return pointer points to the char position in the given char array
 * after reading one seralized string of WindowValue.
 *
 * Note that 'value' is allocated by the caller.
 */
static char *
deserializeValue(char *read_pos, WindowValue * value,
				 bool byvalue, int16 typelen)
{
	memcpy(&(value->valueIsNull), read_pos, sizeof(bool));
	read_pos += sizeof(bool);

	if (!value->valueIsNull)
	{
		if (byvalue)
		{
			memcpy(&(value->value), read_pos, sizeof(Datum));
			read_pos += sizeof(Datum);
		}
		else
		{
			/*
			 * Make read_pos properly aligned, since it is the start address
			 * of a Datum that is passed by reference.
			 */
			read_pos = (char *)MAXALIGN(read_pos);

			value->value = PointerGetDatum(read_pos);
			Size		valueLen = datumGetSize(value->value, byvalue, typelen);

			read_pos += valueLen;
		}
	}
	else
		value->value = 0;

	return read_pos;
}

/*
 * deserializeFuncs -- deserialize Datum values from a given char array
 * for all functions in a key level.
 */
static List *
deserializeFuncs(WindowStatePerLevel level_state,
				 FrameBufferEntry *entry_buf,
				 char *start_pos,
				 Size len)
{
	List	   *func_values = entry_buf->func_values;

	ListCell   *lc;
	char	   *read_pos = start_pos;

	/* the value for each function. */
	foreach(lc, level_state->level_funcs)
	{
		WindowStatePerFunction funcstate = (WindowStatePerFunction) lfirst(lc);
		WindowValue *value;

		/* skip window functions with trivial frames */
		if (funcstate->trivial_frame)
			continue;

		/* functions that require peer counts. */
		else if (funcstate->winpeercount)
		{
			int16		typelen;
			bool		byvalue;

			get_typlenbyval(CUME_DIST_PRELIM_TYPE,
							&typelen, &byvalue);
			value = list_nth(func_values, funcstate->serial_index);
			read_pos = deserializeValue(read_pos, value, byvalue, typelen);
		}
		else if (IS_LEAD_LAG(funcstate->wfuncstate->winkind) ||
				 IS_FIRST_LAST(funcstate->wfuncstate->winkind))
		{
			WindowFuncExprState *wfxstate;
			int			nargs;
			int			argno = 1;

			wfxstate = funcstate->wfuncstate;
			nargs = list_length(wfxstate->args);
			Assert(2 <= nargs && nargs <= 4);

			value = list_nth(func_values, funcstate->serial_index);
			read_pos = deserializeValue(read_pos, value,
										wfxstate->argtypbyval[argno],
										wfxstate->argtyplen[argno]);
		}
		else if (OidIsValid(funcstate->invprelimfn_oid) ||
				 OidIsValid(funcstate->prelimfn_oid))
		{
			/*
			 * The function which has an inverse preliminary function.
			 */
			value = list_nth(func_values, funcstate->serial_index);

			/*
			 * Read number of not nulls if this function has a preliminary
			 * function.
			 */
			if (OidIsValid(funcstate->invprelimfn_oid))
			{
				memcpy(&(value->numNotNulls), read_pos, sizeof(uint64));
				read_pos += sizeof(uint64);
			}

			read_pos = deserializeValue(read_pos, value,
										funcstate->aggTranstypeByVal,
										funcstate->aggTranstypeLen);
		}
		else
		{
			/* the general case */
			WindowFuncExprState *winref_state = funcstate->wfuncstate;
			ListCell   *ref_lc;
			Oid			typid;
			bool		byval;
			int16		arglen;
			int			argno = 0;

			foreach(ref_lc, winref_state->args)
			{
				ExprState  *argstate = (ExprState *)lfirst(ref_lc);

				typid = exprType((Node *) argstate->expr);
				get_typlenbyval(typid, &arglen, &byval);

				value = list_nth(func_values, funcstate->serial_index + argno);
				read_pos = deserializeValue(read_pos, value, byval, arglen);

				argno++;
			}
		}
	}

	Assert((read_pos - start_pos) == len);

	return func_values;
}

/*
 * serializeEntry -- construct a serialized version of function values for
 * a given key level using the given char array.
 *
 * If the data generated by this serialization have more bytes than the space
 * provided by the given char array, the size of this array is increased
 * in this function.
 */
static void
serializeEntry(WindowStatePerLevel level_state,
			   ExprContext *econtext,
			   StringInfo serial_buf)
{
	resetStringInfo(serial_buf);

	/* We rely on the address of the char array is maxaligned. */
	Assert(serial_buf->len == MAXALIGN(serial_buf->len));

	if (!EDGE_IS_ROWS(level_state))
	{
		int			key_no;
		TupleTableSlot *slot = econtext->ecxt_outertuple;

		/* Copy the keys */
		for (key_no = 0; key_no < level_state->numSortCols; key_no++)
		{
			AttrNumber	attnum = level_state->sortColIdx[key_no];
			Datum		key;
			bool		isnull;

			key = slot_getattr(slot, attnum, &isnull);

			serializeValue(key, isnull,
						   level_state->col_typbyvals[key_no],
						   level_state->col_typlens[key_no],
						   serial_buf);
		}
	}

	/* Copy function values */
	serializeFuncs(level_state,
				   econtext,
				   serial_buf);
}

/*
 * deserializeEntry -- deserialize a buffer entry.
 */
static FrameBufferEntry *
deserializeEntry(WindowStatePerLevel level_state,
				 FrameBufferEntry *entry_buf,
				 char *serial_entry, Size len)
{
	FrameBufferEntry *entry = entry_buf;
	char	   *read_pos = serial_entry;
	Size		keylen = 0;
	int			key_no;

	/* We rely on the address of serial_entry is maxaligned. */
	Assert(serial_entry == (char *)MAXALIGN(serial_entry));

	if (!EDGE_IS_ROWS(level_state))
	{
		for (key_no = 0; key_no < level_state->numSortCols; key_no++)
		{
			Size		curr_keylen = 0;

			memcpy(&(entry->nulls[key_no]), read_pos, sizeof(bool));
			read_pos += sizeof(bool);
			keylen += sizeof(bool);

			if (!entry->nulls[key_no])
			{
				if (level_state->col_typbyvals[key_no])
				{
					memcpy(&(entry->keys[key_no]), read_pos, sizeof(Datum));
					curr_keylen = sizeof(Datum);
				}

				else
				{
					Size		alignmentBytes = ((char *)MAXALIGN(read_pos)) - read_pos;

					read_pos += alignmentBytes;
					keylen += alignmentBytes;

					curr_keylen = datumGetSize(PointerGetDatum(read_pos),
										  level_state->col_typbyvals[key_no],
										   level_state->col_typlens[key_no]);
					entry->keys[key_no] = PointerGetDatum(read_pos);
				}
				read_pos += curr_keylen;
				keylen += curr_keylen;
			}
		}
	}

	/* deserial function values */
	entry->func_values = deserializeFuncs(level_state, entry_buf,
										  read_pos, len - keylen);

	return entry;
}

/*
 * adjustEdgesAfterAppend -- adjust the trailing and leading edges for
 * a given level_state after appending a value into its frame buffer.
 */
static void
adjustEdgesAfterAppend(WindowStatePerLevel level_state,
					   WindowState * wstate,
					   bool last_peer)
{
	WindowFrameBuffer buffer = level_state->frame_buffer;
	ExprContext *econtext = wstate->ps.ps_ExprContext;
	TupleTableSlot *inserting_tuple = econtext->ecxt_outertuple;

	/*
	 * If the current_row_reader for the buffer is not set, set it to point to
	 * the last row.
	 */
	if (!ntuplestore_acc_tell(buffer->current_row_reader, NULL))
		ntuplestore_acc_seek_last(buffer->current_row_reader);

	/*
	 * Adjust the trailing edge if it is "ROWS x FOLLOWING" or it is a delayed
	 * edge.
	 */
	if (!(level_state->empty_frame && level_state->has_delay_trail))
	{
		if (EDGE_IS_ROWS(level_state) &&
			EDGE_TRAIL_IS_BOUND(level_state) &&
			level_state->trail_rows > 0)
		{
			if (!ntuplestore_acc_tell(level_state->trail_reader, NULL))
			{
				if (!last_peer)
					ntuplestore_acc_seek_last(level_state->trail_reader);
			}
			else if (level_state->num_trail_rows < level_state->trail_rows - 1)
			{
				ntuplestore_acc_advance(level_state->trail_reader, 1);
				level_state->num_trail_rows++;
			}
		}
		else if (!EDGE_IS_ROWS(level_state) &&
				 (EDGE_TRAIL_IS_BOUND_FOLLOWING(level_state) ||
				  level_state->has_delay_trail))
		{
			econtext->ecxt_outertuple = wstate->curslot;
			forwardEdgeForRange(level_state, wstate,
								level_state->trail_expr,
								level_state->trail_range_expr,
								level_state->trail_reader,
								EDGE_TRAIL);

			econtext->ecxt_outertuple = inserting_tuple;
		}
	}


	if (!(level_state->empty_frame && level_state->has_delay_lead))
	{
		/* If the leading edge is "x PRECEDING" and x > 0, simply return. */
		if (EDGE_LEAD_IS_BOUND_PRECEDING(level_state) &&
			ntuplestore_acc_tell(level_state->lead_reader, NULL))
		{
			level_state->lead_ready = true;
			return;
		}

		/*
		 * Adjust the leading edge when the edge is "x FOLLOWING" or "CURRENT
		 * ROW".
		 */
		if (EDGE_IS_ROWS(level_state))
		{
			if (level_state->num_lead_rows < level_state->lead_rows)
			{
				Assert(!ntuplestore_acc_tell(level_state->lead_reader, NULL));

				if (!last_peer)
					level_state->num_lead_rows++;
			}
			else if ((EDGE_LEAD_IS_BOUND(level_state) ||
					  EDGE_LEAD_EQ_CURRENT_ROW(level_state, wstate)) &&
					 level_state->num_lead_rows == level_state->lead_rows)
			{
				/* If the leading edge is not set, set it here. */
				if (!ntuplestore_acc_tell(level_state->lead_reader, NULL))
					ntuplestore_acc_seek_last(level_state->lead_reader);
			}
			else if (!EDGE_LEAD_IS_BOUND(level_state) &&
					 !EDGE_LEAD_EQ_CURRENT_ROW(level_state, wstate))
			{
				if (!ntuplestore_acc_tell(level_state->lead_reader, NULL))
					ntuplestore_acc_seek_last(level_state->lead_reader);
				else
				{
					bool		found = ntuplestore_acc_advance(level_state->lead_reader, 1);

					if (found)
						level_state->num_lead_rows++;
				}
			}
		}
		else
		{
			if (EDGE_LEAD_EQ_CURRENT_ROW(level_state, wstate))
			{
				bool		found;
				NTupleStorePos pos;

				found = ntuplestore_acc_tell(buffer->current_row_reader, &pos);
				Assert(found);
				ntuplestore_acc_seek(level_state->lead_reader, &pos);

				level_state->lead_ready = true;
			}
			else if (EDGE_LEAD_IS_BOUND(level_state))
			{
				econtext->ecxt_outertuple = wstate->curslot;
				forwardEdgeForRange(level_state, wstate,
									level_state->lead_expr,
									level_state->lead_range_expr,
									level_state->lead_reader,
									EDGE_LEAD);
				econtext->ecxt_outertuple = inserting_tuple;
			}
			else
			{
				ntuplestore_acc_advance(level_state->lead_reader, 1);
				level_state->lead_ready = last_peer;
			}
		}
	}
}

/*
 * appendToFrameBuffer -- append the intermediate values stored in level
 */
static void
appendToFrameBuffer(WindowStatePerLevel level_state,
					WindowState * wstate,
					bool last_peer)
{
	WindowFrameBuffer buffer = level_state->frame_buffer;
	ExprContext *econtext = wstate->ps.ps_ExprContext;

	Assert(buffer->is_rows == EDGE_IS_ROWS(level_state));
	resetStringInfo(&level_state->serial_buf);
	serializeEntry(level_state, econtext,
				   &(level_state->serial_buf));

	ntuplestore_acc_put_data(buffer->writer,
							 (void *)(level_state->serial_buf.data),
							 level_state->serial_buf.len);

	adjustEdgesAfterAppend(level_state, wstate, last_peer);

	buffer->num_rows_after++;
}

/*
 * trimFrameBuffer -- trim the old values in the buffer.
 */
static void
trimFrameBuffer(WindowFrameBuffer buffer)
{
	NTupleStorePos pos;
	bool		found;

	Assert(buffer != NULL);

	found = ntuplestore_acc_tell(buffer->trim_reader, &pos);
	if (!found)
		return;

	ntuplestore_trim(buffer->tuplestore, &pos);
}

/*
 * getCurrentValue -- obtain the value at the current position for
 * an accessor.
 *
 * The value is stored in the given 'entry_buf'. Note that 'entry_buf'
 * is allocated by the caller.
 *
 * If the current position of the accessor is an invalid position, this
 * function returns false.
 */
static bool
getCurrentValue(NTupleStoreAccessor * reader,
				WindowStatePerLevel level_state,
				FrameBufferEntry *entry_buf)
{
	void	   *data;
	int			len;

	bool		found = ntuplestore_acc_current_data(reader, &data, &len);

	if (!found)
		return false;

	entry_buf = deserializeEntry(level_state, entry_buf, data, len);

	return true;
}

/*
 * incrementCurrentRow -- increment the current_row in the buffer.
 */
static void
incrementCurrentRow(WindowFrameBuffer buffer,
					WindowState * wstate)
{
	WindowStatePerLevel level_state = buffer->level_state;

	/* Increment the current_row_reader */
	ntuplestore_acc_advance(buffer->current_row_reader, 1);

	buffer->num_rows_before++;
	if (buffer->num_rows_after > 0)
		buffer->num_rows_after--;

	if (EDGE_IS_ROWS(level_state))
	{
		level_state->num_trail_rows--;
		level_state->num_lead_rows--;
	}

	if (!buffer->level_state->trivial_frames_only)
	{
		/* Adjust all leading and trailig edges */
		adjustEdges(buffer, wstate);
	}

	/* Set the trim_reader, and trim the buffer */
	if (!level_state->has_delay_bound)
	{
		NTupleStorePos pos;
		bool		found = ntuplestore_acc_tell(buffer->current_row_reader, &pos);

		if (found)
			ntuplestore_acc_seek(buffer->trim_reader, &pos);
		else
			ntuplestore_acc_set_invalid(buffer->trim_reader);

		if (ntuplestore_acc_tell(level_state->trail_reader, NULL) &&
			((ntuplestore_acc_tell(buffer->trim_reader, NULL) &&
			  ntuplestore_acc_is_before(level_state->trail_reader,
										buffer->trim_reader)) ||
			 !ntuplestore_acc_tell(buffer->trim_reader, NULL)))
		{
			found = ntuplestore_acc_tell(level_state->trail_reader, &pos);
			Assert(found);
			ntuplestore_acc_seek(buffer->trim_reader, &pos);
		}

		if (!ntuplestore_acc_tell(level_state->trail_reader, NULL))
			ntuplestore_acc_set_invalid(buffer->trim_reader);
		else
			ntuplestore_acc_advance(buffer->trim_reader, -1);

		trimFrameBuffer(buffer);
	}
}

/*
 * hasEnoughDataInRange -- return true if there is enough data in
 * the buffer that satisfy a given value range.
 */
static bool
hasEnoughDataInRange(WindowFrameBuffer buffer,
					 WindowStatePerLevel level_state,
					 WindowState * wstate,
					 Datum trail_range, Datum lead_range)
{
	if (level_state->empty_frame)
		return true;

	if (!EDGE_LEAD_IS_BOUND(level_state) &&
		!EDGE_LEAD_EQ_CURRENT_ROW(level_state, wstate))
		return false;

	return level_state->lead_ready;
}


/*
 * hasEnoughDataInRows -- return true if there is enough data in the
 * buffer that satisfy	a given row range.
 */
static bool
hasEnoughDataInRows(WindowFrameBuffer buffer,
					WindowStatePerLevel level_state,
					WindowState * wstate,
					int64 trail_rows, int64 lead_rows)
{
	if (level_state->empty_frame)
		return true;

	if (!EDGE_LEAD_IS_BOUND(level_state) &&
		!EDGE_LEAD_EQ_CURRENT_ROW(level_state, wstate))
		return false;

	if (trail_rows == 0 &&
		lead_rows == 0)
		return true;

	if (level_state->num_lead_rows >= lead_rows)
		return true;
	return false;
}

static FrameBufferEntry *
createFrameBufferEntry(WindowStatePerLevel level_state)
{
	FrameBufferEntry *entry = NULL;
	ListCell   *lc;

	entry = (FrameBufferEntry *)palloc0(sizeof(FrameBufferEntry));
	if (!EDGE_IS_ROWS(level_state))
	{
		entry->keys = (Datum *)palloc0(level_state->numSortCols * sizeof(Datum));
		entry->nulls = (bool *)palloc0(level_state->numSortCols * sizeof(bool));
	}

	entry->func_values = NIL;

	/* Allocate the space for WindowValue */
	foreach(lc, level_state->level_funcs)
	{
		WindowStatePerFunction funcstate = (WindowStatePerFunction) lfirst(lc);
		WindowValue *value;

		/* skip window functions with trivial frames */
		if (funcstate->trivial_frame)
			continue;

		else if (funcstate->winpeercount ||
				 (IS_LEAD_LAG(funcstate->wfuncstate->winkind) ||
				  IS_FIRST_LAST(funcstate->wfuncstate->winkind)) ||
				 (OidIsValid(funcstate->invprelimfn_oid) ||
				  OidIsValid(funcstate->prelimfn_oid)))
		{
			value = (WindowValue *) palloc0(sizeof(WindowValue));
			entry->func_values = lappend(entry->func_values, value);
		}

		else
		{
			WindowFuncExprState *winref_state = funcstate->wfuncstate;
			int			argno;

			for (argno = 0; argno < list_length(winref_state->args); argno++)
			{
				value = (WindowValue *) palloc0(sizeof(WindowValue));
				entry->func_values = lappend(entry->func_values, value);
			}
		}
	}

	return entry;
}

static void
freeFrameBufferEntry(FrameBufferEntry *entry)
{
	if (entry->keys != NULL)
		pfree(entry->keys);

	if (entry->nulls != NULL)
		pfree(entry->nulls);

	if (entry->func_values != NULL)
		list_free_deep(entry->func_values);
}

/*
 * hasTuplesInFrame - Check if there are tuples between the trailing
 * and leading frame edge for the current_row.
 *
 * This function returns true if there are such tuples. Otherwise,
 * return false.
 */
static bool
hasTuplesInFrame(WindowStatePerLevel level_state,
				 WindowState * wstate)
{
	bool		has_tuples = true;
	NTupleStorePos orig_pos;
	bool		trail_valid;
	ExprContext *econtext = wstate->ps.ps_ExprContext;

	if (level_state->empty_frame)
		return false;

	/*
	 * If both trail_reader and lead_reader point to the same position, there
	 * are no tuples within the frame edges.
	 */
	if (ntuplestore_acc_tell(level_state->trail_reader, NULL) &&
		ntuplestore_acc_tell(level_state->lead_reader, NULL) &&
		(!ntuplestore_acc_is_before(level_state->lead_reader,
									level_state->trail_reader) &&
		 !ntuplestore_acc_is_before(level_state->trail_reader,
									level_state->lead_reader)))
		return false;

	/* Save the position for the trail_reader */
	trail_valid = ntuplestore_acc_tell(level_state->trail_reader, &orig_pos);

	if (trail_valid)
		ntuplestore_acc_advance(level_state->trail_reader, 1);
	else
	{
		/*
		 * When the leading edge and trailing edge are both RANGE x PRECEDING,
		 * and the trail_reader points to an invalid position but the
		 * lead_reader points to a valid position, it is possible that there
		 * are no tuples in the current frame.
		 */
		if (!EDGE_IS_ROWS(level_state) &&
			EDGE_LEAD_IS_BOUND_PRECEDING(level_state) &&
			EDGE_TRAIL_IS_BOUND_PRECEDING(level_state) &&
			ntuplestore_acc_tell(level_state->lead_reader, NULL) &&
			!ntuplestore_acc_tell(level_state->trail_reader, NULL))
		{
			bool		is_less = true;
			MemoryContext oldctx;
			Datum		trail_edge_value;
			bool		trail_edge_value_isnull;
			bool		has_entry;
			FrameBufferEntry *entry = level_state->curr_entry_buf;

			oldctx = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);
			trail_edge_value =
				ExecEvalExpr(level_state->trail_range_expr, econtext,
							 &(trail_edge_value_isnull), NULL);

			has_entry = getCurrentValue(level_state->lead_reader, level_state, entry);
			Assert(has_entry);
			Assert(level_state->numSortCols <= 1);

			is_less = cmp_deformed_tuple(entry->keys, entry->nulls,
										 &trail_edge_value,
										 &trail_edge_value_isnull,
										 level_state->numSortCols,
										 level_state->ltfunctions,
										 level_state->eqfunctions,
										 wstate->cmpcontext,
										 false);
			MemoryContextSwitchTo(oldctx);

			return (!is_less);
		}

		if (!EDGE_TRAIL_IS_BOUND_FOLLOWING(level_state) ||
			EDGE_TRAIL_EQ_CURRENT_ROW(level_state, wstate))
			ntuplestore_acc_seek_first(level_state->trail_reader);
	}

	/*
	 * When both trail_reader and lead_reader points to an entry in the frame
	 * buffer, we compare their position order.
	 */
	if (ntuplestore_acc_tell(level_state->trail_reader, NULL) &&
		ntuplestore_acc_tell(level_state->lead_reader, NULL) &&
		ntuplestore_acc_is_before(level_state->lead_reader,
								  level_state->trail_reader))
		has_tuples = false;

	/*
	 * When the trail_reader does not point to any entry in the frame buffer,
	 * and the trailing edge is x FOLLOWING, it is possible that there are no
	 * tuples within frame edges for the current_row.
	 */
	else if (!ntuplestore_acc_tell(level_state->trail_reader, NULL) &&
			 EDGE_TRAIL_IS_BOUND_FOLLOWING(level_state))
	{
		if (EDGE_IS_ROWS(level_state))
		{
			if (level_state->num_lead_rows < level_state->trail_rows)
				has_tuples = false;
		}

		else
		{
			if (!ntuplestore_acc_tell(level_state->trail_reader, NULL) &&
				!exec_eq_exprstate(wstate, level_state->trail_range_eq_expr))
			{
				Datum		new_edge_value = 0;
				bool		new_edge_value_isnull = false;
				bool		in_trail_edge = false;
				bool		in_lead_edge = false;
				MemoryContext oldctx;

				has_tuples = false;

				/* Check if the last row is within the edges */

				oldctx = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);
				new_edge_value =
					ExecEvalExpr(level_state->trail_range_expr, econtext,
								 &(new_edge_value_isnull), NULL);
				MemoryContextSwitchTo(oldctx);

				in_trail_edge =
					checkLastRowForEdge(level_state, wstate,
										level_state->trail_reader,
										new_edge_value,
										new_edge_value_isnull,
										EDGE_TRAIL);

				in_lead_edge = true;
				if (level_state->lead_range_expr != NULL)
				{
					oldctx = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);
					new_edge_value =
						ExecEvalExpr(level_state->lead_range_expr, econtext,
									 &(new_edge_value_isnull), NULL);
					MemoryContextSwitchTo(oldctx);

					in_lead_edge =
						checkLastRowForEdge(level_state, wstate,
											level_state->lead_reader,
											new_edge_value,
											new_edge_value_isnull,
											EDGE_LEAD);
				}

				if (in_trail_edge && in_lead_edge)
					has_tuples = true;
			}
		}
	}

	/*
	 * When the lead_reader does not point to any entry in the frame buffer,
	 * and the leading edge is x PRECEDING, it means that there are no tuples
	 * within the frame edges for the current_row.
	 */
	else if (!ntuplestore_acc_tell(level_state->lead_reader, NULL) &&
			 (EDGE_LEAD_IS_BOUND_PRECEDING(level_state) &&
			  ((EDGE_IS_ROWS(level_state) && level_state->lead_rows != 0) ||
			   (!EDGE_IS_ROWS(level_state) && !exec_eq_exprstate(wstate,
										 level_state->lead_range_eq_expr)))))
		has_tuples = false;

	/* Restore trail_reader */
	if (trail_valid)
		ntuplestore_acc_seek(level_state->trail_reader, &orig_pos);
	else
		ntuplestore_acc_set_invalid(level_state->trail_reader);

	return has_tuples;
}

/*
 * freeTransValue -- release the space allocated for the given transition value.
 */
static void
freeTransValue(Datum *transValue,
			   bool transTypeByVal,
			   bool *transValueIsNull,
			   bool *noTransValue,
			   bool shouldFree)
{
	if (!shouldFree)
		return;

	if (!transTypeByVal && !(*noTransValue) && !(*transValueIsNull) &&
		DatumGetPointer(*transValue) != NULL)
		pfree(DatumGetPointer(*transValue));

	*transValue = 0;
	*transValueIsNull = true;
	*noTransValue = true;
}

/*
 * computeTransValuesThroughScan -- compute transition values
 * for those functions in the given level whose aggregate values
 * can only be computed through scanning through multiple entries
 * in the frame buffer.
 */
static void
computeTransValuesThroughScan(WindowStatePerLevel level_state,
							  WindowState * wstate)
{
	/* Indicate if the current frame contains tuples at all. */
	bool		has_tuples = true;
	ListCell   *lc;
	bool		has_curr_entry = false;
	FrameBufferEntry *curr_entry = level_state->curr_entry_buf;
	WindowValue *curr_value = NULL;
	ExprContext *econtext = wstate->ps.ps_ExprContext;
	FunctionCallInfoData fcinfo;
	NTupleStorePos orig_pos;

	has_tuples = hasTuplesInFrame(level_state, wstate);

	/* Remember the original trail_reader position. */
	ntuplestore_acc_tell(level_state->trail_reader, &orig_pos);

	/*
	 * Since the trail_reader points to the value before the trailing edge, we
	 * advance the trail_reader by one.
	 */
	if (ntuplestore_acc_tell(level_state->trail_reader, NULL))
		ntuplestore_acc_advance(level_state->trail_reader, 1);
	else
	{
		if (!ntuplestore_acc_tell(level_state->lead_reader, NULL) &&
			(EDGE_LEAD_IS_BOUND_PRECEDING(level_state) &&
			 ((EDGE_IS_ROWS(level_state) && level_state->lead_rows != 0) ||
			  (!EDGE_IS_ROWS(level_state) && !exec_eq_exprstate(wstate,
										 level_state->lead_range_eq_expr)))))
			has_tuples = false;
		else
			ntuplestore_acc_seek_first(level_state->trail_reader);
	}

	foreach(lc, level_state->level_funcs)
	{
		WindowStatePerFunction funcstate = (WindowStatePerFunction)
		lfirst(lc);

		/*
		 * Ignore those functions that do not require scanning multiple
		 * entries in the frame buffer.
		 */
		if (funcstate->trivial_frame ||
			funcstate->winpeercount ||
			(funcstate->plain_agg && OidIsValid(funcstate->invprelimfn_oid)) ||
			!funcstate->plain_agg)
			continue;

		freeTransValue(&funcstate->final_aggTransValue,
					   funcstate->aggTranstypeByVal,
					   &funcstate->final_aggTransValueIsNull,
					   &funcstate->final_aggNoTransValue,
					   funcstate->final_aggShouldFree);

		funcstate->final_aggTransValue =
			datumCopyWithMemManager(0, funcstate->aggInitValue,
									funcstate->aggTranstypeByVal,
									funcstate->aggTranstypeLen,
									&(wstate->mem_manager));
		funcstate->final_aggTransValueIsNull = funcstate->aggInitValueIsNull;
		funcstate->final_aggNoTransValue = funcstate->aggInitValueIsNull;
		funcstate->final_aggShouldFree = !funcstate->aggInitValueIsNull;
	}

	if (has_tuples)
	{
		bool		include_last_agg = false;

		while (ntuplestore_acc_tell(level_state->trail_reader, NULL))
		{
			if (ntuplestore_acc_tell(level_state->lead_reader, NULL) &&
				ntuplestore_acc_is_before(level_state->lead_reader,
										  level_state->trail_reader))
				break;

			has_curr_entry = getCurrentValue(level_state->trail_reader,
											 level_state, curr_entry);

			Assert(has_curr_entry);

			foreach(lc, level_state->level_funcs)
			{
				WindowStatePerFunction funcstate = (WindowStatePerFunction)
				lfirst(lc);
				FmgrInfo   *fmgr_info;

				/*
				 * Ignore those functions that do not require scanning
				 * multiple entries in the frame buffer.
				 */
				if (funcstate->trivial_frame ||
					funcstate->winpeercount ||
					(funcstate->plain_agg &&
					 OidIsValid(funcstate->invprelimfn_oid)) ||
					!funcstate->plain_agg)
					continue;

				if (OidIsValid(funcstate->prelimfn_oid))
				{
					curr_value =
						(WindowValue *) list_nth(curr_entry->func_values,
												 funcstate->serial_index);

					Assert(curr_value);

					fcinfo.arg[1] = curr_value->value;
					fcinfo.argnull[1] = curr_value->valueIsNull;
					fmgr_info = &(funcstate->prelimfn);
				}

				else
				{
					int			argno;

					for (argno = 1; argno <= funcstate->numargs; argno++)
					{
						curr_value = (WindowValue *)
							list_nth(curr_entry->func_values,
									 funcstate->serial_index + (argno - 1));
						Assert(curr_value);

						fcinfo.arg[argno] = curr_value->value;
						fcinfo.argnull[argno] = curr_value->valueIsNull;
					}
					fmgr_info = &(funcstate->transfn);
				}

				funcstate->final_aggTransValue =
					invoke_agg_trans_func(fmgr_info,
										  fmgr_info->fn_nargs - 1,
										  funcstate->final_aggTransValue,
										  &funcstate->final_aggNoTransValue,
									 &(funcstate->final_aggTransValueIsNull),
										  funcstate->aggTranstypeByVal,
										  funcstate->aggTranstypeLen,
										  &fcinfo, (void *)wstate,
										  econtext->ecxt_per_tuple_memory,
										  &(wstate->mem_manager));
				funcstate->final_aggShouldFree = true;
			}

			ntuplestore_acc_advance(level_state->trail_reader, 1);
		}

		/*
		 * Add the funcstate->aggTransValue if it is in the current frame.
		 */
		if (EDGE_TRAIL_EQ_CURRENT_ROW(level_state, wstate))
			include_last_agg = true;

		if (!EDGE_LEAD_IS_BOUND(level_state) ||
			EDGE_LEAD_IS_BOUND_FOLLOWING(level_state) ||
			EDGE_LEAD_EQ_CURRENT_ROW(level_state, wstate))
			include_last_agg = true;

		if (!ntuplestore_acc_tell(level_state->lead_reader, NULL) &&
			level_state->agg_filled && include_last_agg)
		{
			foreach(lc, level_state->level_funcs)
			{
				WindowStatePerFunction funcstate = (WindowStatePerFunction)
				lfirst(lc);

				/*
				 * Ignore those functions that do not require scanning
				 * multiple entries in the frame buffer.
				 */
				if (funcstate->trivial_frame ||
					funcstate->winpeercount ||
					(funcstate->plain_agg && OidIsValid(funcstate->invprelimfn_oid)) ||
					!funcstate->plain_agg)
					continue;

				if (OidIsValid(funcstate->prelimfn_oid))
				{
					fcinfo.arg[1] = funcstate->aggTransValue;
					fcinfo.argnull[1] = funcstate->aggTransValueIsNull;

					funcstate->final_aggTransValue =
						invoke_agg_trans_func(&(funcstate->prelimfn),
											funcstate->prelimfn.fn_nargs - 1,
											  funcstate->final_aggTransValue,
										   &funcstate->final_aggNoTransValue,
									 &(funcstate->final_aggTransValueIsNull),
											  funcstate->aggTranstypeByVal,
											  funcstate->aggTranstypeLen,
											  &fcinfo, (void *)wstate,
											  econtext->ecxt_per_tuple_memory,
											  &(wstate->mem_manager));
					funcstate->final_aggShouldFree = true;
				}
				else
				{
					TupleTableSlot *slot = econtext->ecxt_outertuple;
					bool		found;

					found = ntuplestore_acc_current_tupleslot(wstate->input_buffer->writer,
															  wstate->spare);
					Assert(found);

					econtext->ecxt_outertuple = wstate->spare;
					add_tuple_to_trans(funcstate, wstate, econtext, false);

					/* Reset back to its orginial value */
					econtext->ecxt_outertuple = slot;
				}
			}
		}
	}

	/* Reset the trail_reader to its original position. */
	if (!ntuplestore_acc_seek(level_state->trail_reader, &orig_pos))
		ntuplestore_acc_set_invalid(level_state->trail_reader);
}

/*
 * computeFrameValue -- compute transition values for each function
 * in a given key level from the data stored in its frame buffer.
 *
 * The 'trail_reader' and 'lead_reader' point to the positions of
 * the trailing and leading edges.
 */
static void
computeFrameValue(WindowStatePerLevel level_state,
				  WindowState * wstate,
				  NTupleStoreAccessor * trail_reader,
				  NTupleStoreAccessor * lead_reader)
{
	ListCell   *lc;
	ExprContext *econtext = wstate->ps.ps_ExprContext;
	WindowFrameBuffer buffer = level_state->frame_buffer;
	FunctionCallInfoData fcinfo;

	FrameBufferEntry *trail_entry = level_state->trail_entry_buf;
	FrameBufferEntry *lead_entry = level_state->lead_entry_buf;
	FrameBufferEntry *curr_entry = level_state->curr_entry_buf;
	bool		has_trail_entry = false;
	bool		has_lead_entry = false;
	bool		has_curr_entry = false;

	WindowValue *trail_value = NULL;
	WindowValue *lead_value = NULL;
	WindowValue *curr_value = NULL;

	bool		read_value = false;
	bool		read_edge_value = false;

	bool		require_scanning = false;

	Assert(EDGE_IS_ROWS(level_state) == buffer->is_rows);

	foreach(lc, level_state->level_funcs)
	{
		WindowStatePerFunction funcstate = (WindowStatePerFunction) lfirst(lc);

		/*
		 * Ignore ordinary window functions, except for functions that require
		 * peer counts.
		 */
		if (funcstate->trivial_frame)
			continue;

		/* Ignore functions that are cumulative aggregate functions. */
		if (funcstate->cumul_frame)
			continue;

		/* functions that require peer counts. */
		else if (funcstate->winpeercount)
		{
			/*
			 * If we didn't read the value pointed by current_row_reader, read
			 * it now.
			 */
			if (!read_value)
			{
				read_value = true;
				has_curr_entry = getCurrentValue(buffer->current_row_reader,
												 level_state, curr_entry);
			}

			if (has_curr_entry)
				curr_value = (WindowValue *) list_nth(curr_entry->func_values,
													funcstate->serial_index);

			if (curr_value != NULL)
			{
				funcstate->win_value = curr_value->value;
				funcstate->win_value_is_null = curr_value->valueIsNull;
			}
			else
			{
				TupleTableSlot *slot = econtext->ecxt_outertuple;

				ntuplestore_acc_current_tupleslot(wstate->input_buffer->writer,
												  wstate->spare);
				econtext->ecxt_outertuple = wstate->spare;
				add_tuple_to_trans(funcstate, wstate, econtext, false);

				/* Reset back to its orginial value */
				econtext->ecxt_outertuple = slot;
			}
		}

		/* Aggregate functions with inverse preliminary functions. */
		else if (funcstate->plain_agg && OidIsValid(funcstate->invprelimfn_oid))
		{
			uint64		trail_num_not_nulls = 0;
			uint64		lead_num_not_nulls = 0;

			if (!read_edge_value)
			{
				read_edge_value = true;

				if (EDGE_IS_ROWS(level_state))
				{
					if (!EDGE_TRAIL_IS_BOUND_FOLLOWING(level_state) ||
						level_state->num_lead_rows >= level_state->num_trail_rows)
					{
						has_trail_entry =
							getCurrentValue(level_state->trail_reader,
											level_state, trail_entry);
						has_lead_entry =
							getCurrentValue(level_state->lead_reader,
											level_state, lead_entry);
					}
				}
				else
				{
					has_trail_entry = getCurrentValue(level_state->trail_reader,
												   level_state, trail_entry);

					/*
					 * If the leading edge is UNBOUNDED FOLLOWING, the value
					 * in the leading edge is funcstate->aggTransValue. We
					 * don't need to read from the buffer.
					 */
					if ((level_state->frameOptions & FRAMEOPTION_END_UNBOUNDED_FOLLOWING) == 0)
					{
						has_lead_entry =
							getCurrentValue(level_state->lead_reader,
											level_state, lead_entry);
					}
				}
			}

			if (!level_state->empty_frame && has_trail_entry)
			{
				Assert(list_length(trail_entry->func_values) >
					   funcstate->serial_index);

				trail_value =
					(WindowValue *) list_nth(trail_entry->func_values,
											 funcstate->serial_index);
			}

			if (!level_state->empty_frame && has_lead_entry)
			{
				Assert(list_length(lead_entry->func_values) >
					   funcstate->serial_index);

				lead_value =
					(WindowValue *) list_nth(lead_entry->func_values,
											 funcstate->serial_index);
			}

			if (trail_value != NULL)
			{
				fcinfo.arg[1] = trail_value->value;
				fcinfo.argnull[1] = trail_value->valueIsNull;
				trail_num_not_nulls = trail_value->numNotNulls;
			}
			else
			{
				fcinfo.arg[1] = funcstate->aggInitValue;
				fcinfo.argnull[1] = funcstate->aggInitValueIsNull;
				trail_num_not_nulls = 0;
			}

			freeTransValue(&funcstate->final_aggTransValue,
						   funcstate->aggTranstypeByVal,
						   &funcstate->final_aggTransValueIsNull,
						   &funcstate->final_aggNoTransValue,
						   funcstate->final_aggShouldFree);

			if (lead_value != NULL || trail_value != NULL)
			{
				bool		has_tuples = hasTuplesInFrame(level_state, wstate);

				if (lead_value != NULL)
				{
					funcstate->final_aggTransValue =
						datumCopyWithMemManager(0, lead_value->value,
												funcstate->aggTranstypeByVal,
												funcstate->aggTranstypeLen,
												&(wstate->mem_manager));
					funcstate->final_aggTransValueIsNull =
						lead_value->valueIsNull;

					funcstate->final_aggNoTransValue = false;
					funcstate->final_aggShouldFree = true;
					lead_num_not_nulls = lead_value->numNotNulls;
				}
				else
				{
					funcstate->final_aggTransValue =
						datumCopyWithMemManager(0, funcstate->aggTransValue,
												funcstate->aggTranstypeByVal,
												funcstate->aggTranstypeLen,
												&(wstate->mem_manager));
					funcstate->final_aggTransValueIsNull =
						funcstate->aggTransValueIsNull;
					funcstate->final_aggNoTransValue = false;
					funcstate->final_aggShouldFree = true;
					lead_num_not_nulls = funcstate->numNotNulls;
				}

				if (has_tuples &&
					(lead_num_not_nulls - trail_num_not_nulls > 0))
				{
					funcstate->final_aggTransValue =
						invoke_agg_trans_func(&(funcstate->invprelimfn),
										 funcstate->invprelimfn.fn_nargs - 1,
											  funcstate->final_aggTransValue,
										   &funcstate->final_aggNoTransValue,
									 &(funcstate->final_aggTransValueIsNull),
											  funcstate->aggTranstypeByVal,
											  funcstate->aggTranstypeLen,
											  &fcinfo, (void *)wstate,
											  econtext->ecxt_per_tuple_memory,
											  &(wstate->mem_manager));
				}

				else
				{
					funcstate->final_aggTransValue = funcstate->aggInitValue;
					funcstate->final_aggTransValueIsNull =
						funcstate->aggInitValueIsNull;
					funcstate->final_aggNoTransValue =
						funcstate->aggInitValueIsNull;
					funcstate->final_aggShouldFree = false;
				}
			}

			else
			{
				/*
				 * Check if the frame overlaps the current row. This is done
				 * by checking if trail is identical to the current row when
				 * the frame is RANGE BETWEEN <following> and <following>, or
				 * if lead is identical to the current row when the frame is
				 * RANGE BETWEEN <preceding> and <preceding>.
				 *
				 * In the case the frame overlaps, take aggTransValue, otherwise
				 * aggInitValue, as the final value.
				 */
				if ((EDGE_TRAIL_IS_BOUND_FOLLOWING(level_state) &&
				   ((EDGE_IS_ROWS(level_state) && level_state->trail_rows != 0) ||
					(!EDGE_IS_ROWS(level_state) &&
					 !exec_eq_exprstate(wstate, level_state->trail_range_eq_expr)))) ||
					(EDGE_LEAD_IS_BOUND_PRECEDING(level_state) &&
					 ((EDGE_IS_ROWS(level_state) && level_state->lead_rows != 0) ||
					  (!EDGE_IS_ROWS(level_state) &&
					   !exec_eq_exprstate(wstate, level_state->lead_range_eq_expr)))))
				{
					funcstate->final_aggTransValue = funcstate->aggInitValue;
					funcstate->final_aggTransValueIsNull = funcstate->aggInitValueIsNull;
					funcstate->final_aggNoTransValue = funcstate->aggInitValueIsNull;
					funcstate->final_aggShouldFree = false;
				}
				else
				{
					funcstate->final_aggTransValue = funcstate->aggTransValue;
					funcstate->final_aggTransValueIsNull = funcstate->aggTransValueIsNull;
					funcstate->final_aggNoTransValue = funcstate->aggNoTransValue;
					funcstate->final_aggShouldFree = false;
				}
			}
		}
		else if (funcstate->plain_agg && OidIsValid(funcstate->prelimfn_oid))
		{
			require_scanning = true;
		}
		else
		{
			/*
			 * Here are the general cases. The frame buffer stores the input
			 * arguments. For window aggregate functions, we scan through
			 * values between the trailing edge and the leading edge, and add
			 * them into the transition value. For ordinary window functions,
			 * we pass the frame buffer to its function handler to generate
			 * the output.
			 */
			if (!funcstate->plain_agg)
			{
				int			argno;
				FunctionCallInfoData fcinfo;

				argno = initFcinfo(funcstate->wfuncstate, &fcinfo, funcstate,
								   econtext, false);

				InitFunctionCallInfoData(fcinfo,
										 &funcstate->flinfo,
										 argno,
										 (void *)funcstate->wfuncstate,
										 NULL);

				funcstate->win_value = FunctionCallInvoke(&fcinfo);
				funcstate->win_value_is_null = fcinfo.isnull;
			}
			else
				require_scanning = true;
		}
	}

	/*
	 * Compute transition values for functions whose values have to be
	 * computed by scanning through multiple entries in the frame buffer. We
	 * will read each entry once.
	 */
	if (require_scanning)
		computeTransValuesThroughScan(level_state, wstate);
}

/*
 * Initialize function state for each function in the Window node.
 */
static void
initWindowFuncState(WindowState * wstate, WindowAgg *node)
{
	int			numrefs;
	int			refno;
	ListCell   *lc;

	if (wstate->wfxstates == NULL)
		return;

	wstate->numfuncs = 0;
	numrefs = list_length(wstate->wfxstates);
	Insist(numrefs > 0);

	wstate->func_state = palloc0(sizeof(WindowStatePerFunctionData) * numrefs);

	/* Initialize per window ref (both ordinary and aggregate derived). */
	refno = -1;
	foreach(lc, wstate->wfxstates)
	{
		WindowFuncExprState *wfuncstate = (WindowFuncExprState *) lfirst(lc);
		WindowFunc *wfunc = (WindowFunc *) wfuncstate->xprstate.expr;
		WindowStatePerFunction perfuncstate;
		Oid			inputTypes[FUNC_MAX_ARGS];
		Form_pg_proc proform;
		HeapTuple	heap_tuple;
		ListCell   *lcarg;
		int			i  ,
					funcno;
		bool		isAgg,
					isWin;
		AclResult	aclresult;

		refno++;				/* First one is 0 */

		/* Register the parent window state with the window ref state node. */
		wfuncstate->windowstate = wstate;

		/* Look for a previous duplicate window function */
		for (funcno = 0; funcno < refno; funcno++)
		{
			WindowFunc  *w = wstate->func_state[funcno].wfunc;

			if (equal(wfunc, w) &&
				!contain_volatile_functions((Node *) w))
				break;
		}
		if (funcno < refno)
		{
			/* Found a match to an existing entry, so just mark it */
			wfuncstate->funcno = funcno;
			continue;
		}

		/*
		 * No match so this window ref represents a new function and we need
		 * to fill in its function state.
		 */
		perfuncstate = &wstate->func_state[funcno];
		perfuncstate->wfuncstate = wfuncstate;
		perfuncstate->wfunc = wfunc;

		perfuncstate->serial_index = -1;

		if (!wstate->trivial_frame)
		{
			WindowStatePerLevel level_state = wstate->level_state;

			/*
			 * When this level has the range frame, this frame may not always
			 * match with the row_number function. For example, row_number()
			 * over (order by cn), sum(qty) over (order by cn) Both of these
			 * functions point to the level "order by cn", which is used the
			 * default frame "range between unbounded preceding and
			 * current_row". This is not valid for row_number. We should not
			 * put row_number and sum in the same level. We can simply ignore
			 * row_number in this key level.
			 */
			if (!EDGE_IS_ROWS(level_state) &&
				wfunc->winfnoid == ROW_NUMBER_OID)
			{
				perfuncstate->trivial_frame = true;
			}
			else
			{
				level_state->level_funcs = lappend(level_state->level_funcs,
												   perfuncstate);
			}
		}
		else
			perfuncstate->trivial_frame = true;

		/* Mark WindowFunc state node with assigned index in the result array */
		wfuncstate->funcno = funcno;
		wstate->numfuncs = funcno + 1;

		/* Check permission to call window function */
		aclresult = pg_proc_aclcheck(wfunc->winfnoid, GetUserId(),
									 ACL_EXECUTE);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, ACL_KIND_PROC,
						   get_func_name(wfunc->winfnoid));

		/* Collect information about the window function's pg_proc entry. */
		heap_tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(wfunc->winfnoid));
		if (!HeapTupleIsValid(heap_tuple))
			elog(ERROR, "cache lookup failed for window function proc %u",
				 wfunc->winfnoid);
		proform = (Form_pg_proc) GETSTRUCT(heap_tuple);

		isAgg = proform->proisagg;
		isWin = proform->proiswindow;
		Assert(!proform->proretset);
		ReleaseSysCache(heap_tuple);

		Assert(isAgg != isWin);
		Assert(isAgg == wfunc->winagg);

		/*
		 * Get actual datatypes of the inputs.	These could be different from
		 * the declared input types, when the function accepts ANY, ANYARRAY
		 * or ANYELEMENT.
		 */
		i = 0;
		foreach(lcarg, wfunc->args)
		{
			inputTypes[i++] = exprType((Node *) lfirst(lcarg));
		}
		perfuncstate->numargs = list_length(wfunc->args);

		perfuncstate->plain_agg = isAgg;

		/* The rest depends on the type (agg-derived or ordinary). */
		if (isAgg)
		{
			Insist(!wstate->trivial_frame);
			initialize_peragg(wstate, wfunc, perfuncstate);
			wfuncstate->winkind = WINKIND_AGGREGATE;
		}
		else if (isWin)
		{
			HeapTuple	win_tuple;
			Form_pg_window winform;
			Oid			windowfn_oid = InvalidOid;
			Const	   *refptr;
			ExprState  *xtrastate;

			win_tuple = SearchSysCache1(WINFNOID, ObjectIdGetDatum(wfunc->winfnoid));
			if (!HeapTupleIsValid(win_tuple))
				elog(ERROR, "cache lookup failed for window function %u",
					 wfunc->winfnoid);

			winform = (Form_pg_window)GETSTRUCT(win_tuple);

			/*
			 * Lookup ordinary window implementation function info.
			 */
			switch (wfunc->winstage)
			{
				case WINSTAGE_IMMEDIATE:
				case WINSTAGE_ROWKEY:
					windowfn_oid = winform->winfunc;
					break;
				case WINSTAGE_PRELIMINARY:
					windowfn_oid = winform->winprefunc;
					break;
			}

			wfuncstate->winkind = winform->winkind;

			Assert(OidIsValid(windowfn_oid));

			/*
			 * Check that the user has permission to call the implementation function.
			 *
			 * XXX initially all implementation functions are builtin and anyone
			 * can call them, but good to follow form.
			 */
			aclresult = pg_proc_aclcheck(windowfn_oid, GetUserId(),
										 ACL_EXECUTE);
			if (aclresult != ACLCHECK_OK)
				aclcheck_error(aclresult, ACL_KIND_PROC,
							   get_func_name(windowfn_oid));

			perfuncstate->allowframe = winform->winallowframe;

			/* Inject extra argument. */
			refptr = makeNode(Const);
			refptr->consttype = INTERNALOID;
			refptr->constlen = 4;
			refptr->constvalue = PointerGetDatum(wfuncstate);
			refptr->constisnull = false;
			refptr->constbyval = true;

			xtrastate = ExecInitExpr((Expr *)refptr, (PlanState *) wstate);

			wfuncstate->args = lcons(xtrastate, wfuncstate->args);
			fmgr_info(windowfn_oid, &perfuncstate->flinfo);

			/*
			 * Initialize byval and typlen for framed funcs. Of course we're
			 * creating an entry here for the inserted refptr but we do so for
			 * consistency.
			 */
			if (perfuncstate->allowframe || IS_LEAD_LAG(wfuncstate->winkind) ||
				IS_FIRST_LAST(wfuncstate->winkind))
			{
				ListCell   *lc;
				int			numargs = list_length(wfuncstate->args);
				int			argno = 0;

				wfuncstate->argtypbyval = palloc(sizeof(bool) * numargs);
				wfuncstate->argtyplen = palloc(sizeof(int16) * numargs);

				foreach(lc, wfuncstate->args)
				{
					if (argno == 0)
					{
						/* internal arg */
						wfuncstate->argtypbyval[argno] = true;
						wfuncstate->argtyplen[argno] = INTERNALOID;
					}
					else
					{
						Oid			typid = inputTypes[argno - 1];
						int16		len;
						bool		byval;

						get_typlenbyval(typid, &len, &byval);
						wfuncstate->argtypbyval[argno] = byval;
						wfuncstate->argtyplen[argno] = len;
					}
					argno++;
				}
			}

			/*
			 * Only set the peer count if we're referencing an actual window
			 * key level and if the peer count is required. This ensures that
			 * subsequent iterations do not overwrite the peer count flag.
			 */
			if (!perfuncstate->trivial_frame && winform->winpeercount)
			{
				wstate->level_state->need_peercount = winform->winpeercount;
				perfuncstate->winpeercount = winform->winpeercount;
				wstate->need_peercount = winform->winpeercount;
			}

			perfuncstate->win_value = 0;
			perfuncstate->win_value_is_null = true;

			ReleaseSysCache(win_tuple);
		}
		else
		{
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
				errmsg("inappropriate use of function as window function")));
		}
	}
}

/*
 * Initialize WindowStatePerLevel in the Window node.
 */
static void
initWindowStatePerLevel(WindowState * wstate, WindowAgg *node)
{
	TupleDesc	desc;
	int			col_no;
	WindowStatePerLevel lvl = wstate->level_state;
	Oid		   *eqOperators;
	int			i;

	desc = ExecGetResultType(wstate->ps.lefttree);

	lvl->has_delay_bound = false;
	lvl->has_delay_trail = false;
	lvl->has_delay_lead = false;
	lvl->has_only_trans_funcs = false;

	/*
	 * Copy the sort columns for this level. If the window key contains an
	 * empty list of sort columns, we copy the previous non-empty list of
	 * sort columns here. This makes it easier to find keys to be stored
	 * in the frame buffer later.
	 */
	lvl->numSortCols = node->ordNumCols;
	lvl->sortColIdx = (AttrNumber *)palloc0(lvl->numSortCols * sizeof(AttrNumber));
	lvl->sortOperators = (Oid *) palloc0(lvl->numSortCols * sizeof(Oid));

	memcpy(lvl->sortColIdx,
		   node->ordColIdx,
		   node->ordNumCols * sizeof(AttrNumber));
	memcpy(lvl->sortOperators,
		   node->ordOperators,
		   node->ordNumCols * sizeof(Oid));

	/* Find comparison functions */
	eqOperators = (Oid *) palloc(lvl->numSortCols * sizeof(Oid));
	for (i = 0; i < lvl->numSortCols; i++)
	{
		eqOperators[i] = get_equality_op_for_ordering_op(lvl->sortOperators[i]);
	}
	lvl->eqfunctions = execTuplesMatchPrepare(lvl->numSortCols,
											  eqOperators);
	lvl->ltfunctions = get_ltfuncs(lvl->numSortCols,
								   lvl->sortOperators);
	pfree(eqOperators);

	lvl->frameOptions = node->frameOptions;
	lvl->startOffset = node->startOffset;
	lvl->endOffset = node->endOffset;

	lvl->col_types = palloc(sizeof(Oid) * lvl->numSortCols);
	lvl->col_typlens = palloc(sizeof(int2) * lvl->numSortCols);
	lvl->col_typbyvals = palloc(sizeof(bool) * lvl->numSortCols);

	for (col_no = 0; col_no < lvl->numSortCols; col_no++)
	{
		AttrNumber	attnum = lvl->sortColIdx[col_no];
		Form_pg_attribute attr = desc->attrs[attnum - 1];

		lvl->col_types[col_no] = attr->atttypid;
		lvl->col_typlens[col_no] = attr->attlen;
		lvl->col_typbyvals[col_no] = attr->attbyval;
	}
}


/* WindowState constructor.
 */
static WindowState *
makeWindowState(WindowAgg *window, EState *estate)
{
	WindowState *wstate = makeNode(WindowState);

	wstate->ps.plan = (Plan *) window;
	wstate->ps.state = estate;

	wstate->wfxstates = NIL;
	wstate->eqfunctions = NULL;
	wstate->numfuncs = 0;
	wstate->func_state = NULL;

	wstate->row_index = 0;

	if (window->ordNumCols > 0 || window->frameOptions != FRAMEOPTION_DEFAULTS ||
		window->startOffset || window->endOffset)
	{
		wstate->level_state = (WindowStatePerLevel)
			palloc0(sizeof(WindowStatePerLevelData));

		initStringInfo(&wstate->level_state->serial_buf);

		wstate->trivial_frame = false;
	}
	else
	{
		wstate->level_state = NULL;
		wstate->trivial_frame = true;
	}

	wstate->transcontext = AllocSetContextCreate(CurrentMemoryContext,
												 "TransContext",
												 ALLOCSET_DEFAULT_MINSIZE,
												 ALLOCSET_DEFAULT_INITSIZE,
												 ALLOCSET_DEFAULT_MAXSIZE);
	wstate->cmpcontext = AllocSetContextCreate(CurrentMemoryContext,
											   "CmpContext",
											   ALLOCSET_DEFAULT_MINSIZE,
											   ALLOCSET_DEFAULT_INITSIZE,
											   ALLOCSET_DEFAULT_MAXSIZE);

	return wstate;
}

/* [Re]initialize prior to processing a new partition.
 */
static void
initializePartition(WindowState * wstate)
{
	/*
	 * Reset partition primitive values. Note row_index is -1 to flag "before
	 * first row of partition" and so that it increments to 0.
	 */
	wstate->row_index = -1;

	/*
	 * Reset key-level primitive values.
	 */
	if (!wstate->trivial_frame)
	{
		WindowStatePerLevel level_state = wstate->level_state;

		level_state->group_index = 0;
		level_state->prior_non_peer_count = 0;
		level_state->peer_index = 0;
		level_state->peer_count = 0;
		level_state->rank = 1;
		level_state->dense_rank = 1;
		level_state->prior_rank = 1;
		level_state->prior_dense_rank = 1;
	}

	/* Per-partition input buffer management reinitialzation. */
	resetInputBuffer(wstate);
}

/*
 * trimInputBuffer -- trim the old values in the input buffer.
 */
static void
trimInputBuffer(WindowInputBuffer buffer)
{
	/* Trim all values before the current_row_reader position. */
	NTupleStorePos pos;
	bool		found;

	Assert(buffer != NULL);

	found = ntuplestore_acc_tell(buffer->current_row_reader, &pos);
	if (!found)
		return;

	ntuplestore_trim(buffer->tuplestore, &pos);
}

/*
 * Reset an input buffer after a partition boundary has been passed. We
 * must keep the last tuple fetched -- it is the tuple which caused the
 * key break -- and put it in the new buffer.
 *
 * Of course, we could get called to initialise the buffer as well.
 */
static void
resetInputBuffer(WindowState * wstate)
{
	bool		save_firsttuple = false;

	if (wstate->input_buffer)
	{
		save_firsttuple = wstate->input_buffer->part_break;
		if (save_firsttuple)
		{
#ifdef USE_ASSERT_CHECKING
			bool		found =
#endif
			ntuplestore_acc_current_tupleslot(wstate->input_buffer->writer,
											  wstate->spare);

			Assert(found);
			wstate->curslot = ExecCopySlot(wstate->curslot, wstate->spare);
		}
		ntuplestore_destroy_accessor(wstate->input_buffer->writer);
		ntuplestore_destroy_accessor(wstate->input_buffer->current_row_reader);
		ntuplestore_destroy_accessor(wstate->input_buffer->reader);

		ntuplestore_reset(wstate->input_buffer->tuplestore);

		wstate->input_buffer->part_break = false;
		wstate->input_buffer->num_tuples = 0;

		wstate->cur_slot_is_new = false;
		wstate->cur_slot_part_break = false;
		wstate->cur_slot_key_break = wstate->trivial_frame ? 0 : 1;

		resetFrameBuffers(wstate);
	}
	else
	{
		wstate->input_buffer = (WindowInputBuffer) palloc0(sizeof(WindowInputBufferData));
		wstate->input_buffer->tuplestore =
			ntuplestore_create((PlanStateOperatorMemKB((PlanState *) wstate) * 1024L) / 2);		/* correct? */
		createFrameBuffers(wstate);
	}

	/*
	 * Reset transition values.  This has to be done even if we are creating
	 * new buffer because the trans values might remain in case of rescanning.
	 */
	if (!wstate->trivial_frame)
	{
		WindowStatePerLevel level_state = wstate->level_state;

		resetTransValues(level_state, wstate);
	}

	wstate->input_buffer->writer =
		ntuplestore_create_accessor(wstate->input_buffer->tuplestore, true);
	wstate->input_buffer->current_row_reader =
		ntuplestore_create_accessor(wstate->input_buffer->tuplestore, false);
	wstate->input_buffer->reader =
		ntuplestore_create_accessor(wstate->input_buffer->tuplestore, false);

	if (save_firsttuple)
	{
		ntuplestore_acc_put_tupleslot(wstate->input_buffer->writer,
									  wstate->curslot);
		wstate->input_buffer->num_tuples++;

		ntuplestore_acc_seek_first(wstate->input_buffer->current_row_reader);
		ntuplestore_acc_seek_first(wstate->input_buffer->reader);

		wstate->cur_slot_is_new = true;
		wstate->row_index++;
	}
}

static void
freeInputBuffer(WindowState * wstate)
{
	ntuplestore_destroy_accessor(wstate->input_buffer->writer);
	ntuplestore_destroy_accessor(wstate->input_buffer->current_row_reader);
	ntuplestore_destroy(wstate->input_buffer->tuplestore);

	pfree(wstate->input_buffer);
	wstate->input_buffer = NULL;
}

/*
 * Advance values related to computing peer count.
 */
static void
advancePeerCount(WindowStatePerLevel level_state)
{
	level_state->prior_non_peer_count += level_state->peer_count + 1;
	level_state->peer_index = 0;
	level_state->peer_count = 0;
}


/* Advance the "primitive" values that drive the calculation of window
 * functions for the window ordering of the specified level.
 *
 * On entry these values are as set by partition initialization or as
 * left by the	previous call to this function. Per-partition state
 * has not yet been advanced by function nextRow(), our caller.
 *
 * The "primitive" values managed here are read-only outside the window
 * framework implemented by this function, initializePartition(), and
 * nextRow().
 *
 * Note that the content of different key levels is independent.  The
 * function nextRow() is responsible for calling this function as needed
 * for related key levels.
 */

static void
advanceKeyLevelState(WindowState * wstate)
{
	WindowStatePerLevel level_state;
	int64		cur_row_index;

	Assert(!wstate->trivial_frame);

	cur_row_index = wstate->row_index;

	level_state = wstate->level_state;

	/*
	 * The level data is still set for the previous row. Remember the
	 * rank and dense_rank values.
	 */
	level_state->prior_rank = level_state->rank;
	level_state->prior_dense_rank = level_state->dense_rank;

	/* Advance level data for first peer in next group. */
	level_state->group_index++;
	level_state->rank = 1 + cur_row_index;
	level_state->dense_rank = 1 + level_state->group_index;
}

static int
initFcinfo(WindowFuncExprState *wfxstate, FunctionCallInfoData *fcinfo,
		   WindowStatePerFunction funcstate, ExprContext *econtext,
		   bool check_nulls)
{
	ListCell   *lcarg;
	int			argno;
	bool		has_nulls = false;
	MemoryContext oldctx;

	/* If this is an agg, the first arg will be the trans value */
	if (funcstate->plain_agg)
		argno = 1;
	else
		argno = 0;

	/* Switch memory context just once for all args */
	oldctx = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

	foreach(lcarg, wfxstate->args)
	{
		ExprState  *argstate = (ExprState *)lfirst(lcarg);

		fcinfo->arg[argno] = ExecEvalExpr(argstate, econtext,
										  fcinfo->argnull + argno, NULL);
		if (fcinfo->argnull[argno])
			has_nulls = true;
		argno++;
	}
	MemoryContextSwitchTo(oldctx);

	if (check_nulls && !has_nulls)
		funcstate->numNotNulls++;

	return argno;
}

static void
add_tuple_to_trans(WindowStatePerFunction funcstate, WindowState * wstate,
				   ExprContext *econtext, bool check_nulls)
{
	int			argno;
	FunctionCallInfoData fcinfo;
	ExprState  *filter = funcstate->wfuncstate->aggfilter;

	/* Skip anything FILTERed out */
	if (filter)
	{
		bool		isnull;
		Datum		res;

		res = ExecEvalExprSwitchContext(filter, econtext,
										&isnull, NULL);
		if (isnull || !DatumGetBool(res))
			return;
	}

	/* Evaluate function arguments, save in fcinfo. */
	argno = initFcinfo(funcstate->wfuncstate, &fcinfo, funcstate, econtext, check_nulls);

	if (funcstate->plain_agg)
	{
		funcstate->aggTransValue =
			invoke_agg_trans_func(&(funcstate->transfn),
								  funcstate->numargs,
								  funcstate->aggTransValue,
								  &(funcstate->aggNoTransValue),
								  &(funcstate->aggTransValueIsNull),
								  funcstate->aggTranstypeByVal,
								  funcstate->aggTranstypeLen,
								  &fcinfo, (void *)wstate,
								  econtext->ecxt_per_tuple_memory,
								  &(wstate->mem_manager));
	}
	else	/* ordinary window function */
	{
		InitFunctionCallInfoData(fcinfo,
								 &funcstate->flinfo,
								 argno,
								 (void *) funcstate->wfuncstate,
								 NULL);
		if (IS_LEAD_LAG(funcstate->wfuncstate->winkind) ||
			IS_LAST_VALUE(funcstate->wfuncstate->winkind) ||
			(IS_FIRST_VALUE(funcstate->wfuncstate->winkind) &&
			 funcstate->aggNoTransValue))
		{
			int			argno = 1;

			funcstate->aggTransValue =
				datumCopyWithMemManager(0, fcinfo.arg[argno],
									 funcstate->wfuncstate->argtypbyval[argno],
										funcstate->wfuncstate->argtyplen[argno],
										&(wstate->mem_manager));
			funcstate->aggTransValueIsNull = fcinfo.argnull[argno];
			funcstate->aggNoTransValue = false;
		}

		else
		{
			MemoryContext oldctx;

			oldctx = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);
			funcstate->win_value = FunctionCallInvoke(&fcinfo);
			funcstate->win_value_is_null = fcinfo.isnull;
			MemoryContextSwitchTo(oldctx);
		}
	}

}

static void
invokeTrivialFuncs(WindowState * wstate, bool *found)
{
	WindowStatePerLevel level_state = wstate->level_state;
	int			funcno;
	ExprContext *econtext = wstate->ps.ps_ExprContext;

	for (funcno = 0; funcno < wstate->numfuncs; funcno++)
	{
		WindowStatePerFunction funcstate = &wstate->func_state[funcno];

		if (funcstate->trivial_frame)
		{
			econtext->ecxt_outertuple = wstate->curslot;
			add_tuple_to_trans(funcstate, wstate, econtext, false);

			if (funcstate->plain_agg)
			{
				funcstate->final_aggTransValue = funcstate->aggTransValue;
				funcstate->final_aggTransValueIsNull = funcstate->aggTransValueIsNull;
				funcstate->final_aggNoTransValue = funcstate->aggNoTransValue;
				funcstate->final_aggShouldFree = false;
			}
		}
		else if (funcstate->cumul_frame &&
				 !HAS_ONLY_TRANS_FUNC(funcstate))
		{
			WindowFrameBuffer frame_buffer;

			Assert(level_state != NULL);
			frame_buffer = level_state->frame_buffer;
			Assert(frame_buffer != NULL);

			freeTransValue(&funcstate->final_aggTransValue,
						   funcstate->aggTranstypeByVal,
						   &funcstate->final_aggTransValueIsNull,
						   &funcstate->final_aggNoTransValue,
						   funcstate->final_aggShouldFree);

			/*
			 * When the current_row_reader is set, we retrieve the
			 * intermediate accumulated aggregate value from the buffer.
			 */
			if (ntuplestore_acc_tell(frame_buffer->current_row_reader, NULL))
			{
				FrameBufferEntry *entry = level_state->curr_entry_buf;
				bool		has_entry = false;
				WindowValue *value = NULL;

				Assert(funcstate->serial_index != -1);

				has_entry = getCurrentValue(frame_buffer->current_row_reader,
											level_state, entry);
				Assert(has_entry);
				value = (WindowValue *) list_nth(entry->func_values,
												 funcstate->serial_index);
				Assert(value != NULL);
				funcstate->final_aggTransValue =
					datumCopyWithMemManager(0, value->value,
											funcstate->aggTranstypeByVal,
											funcstate->aggTranstypeLen,
											&(wstate->mem_manager));
				funcstate->final_aggTransValueIsNull = value->valueIsNull;
				funcstate->final_aggNoTransValue = false;
				funcstate->final_aggShouldFree = true;
			}

			/*
			 * Otherwise, the value for the current_rows is stored in
			 * aggTransValue.
			 */
			else
			{
				funcstate->final_aggTransValue = funcstate->aggTransValue;
				funcstate->final_aggTransValueIsNull = funcstate->aggTransValueIsNull;
				funcstate->final_aggNoTransValue = funcstate->aggNoTransValue;
				funcstate->final_aggShouldFree = false;
			}
		}

		else
			*found = true;
	}
}

/*
 * cmp_deformed_tuple -- compare two given arrays of Datums.
 *
 * If is_equal is true, this function returns true when these two Datums are
 * equal.
 *
 * If is_equal is false, this function return true if Datum a is ordered
 * before Datum b. For DESC ordering, 'ltFuncs' are actually > operators.
 */
static bool
cmp_deformed_tuple(Datum *a, bool *a_nulls, Datum *b, bool *b_nulls,
				   int ncols, FmgrInfo *ltfuncs,
				 FmgrInfo *eqfuncs, MemoryContext evalContext, bool is_equal)
{
	MemoryContext oldContext;
	bool		result;
	int			i	  ,
				j;
	FmgrInfo   *funcs;

	/* Reset and switch into the temp context. */
	MemoryContextReset(evalContext);
	oldContext = MemoryContextSwitchTo(evalContext);

	if (is_equal)
	{
		funcs = eqfuncs;
		result = true;
	}
	else
	{
		funcs = ltfuncs;
		result = true;
	}

	/*
	 * For equality testing, we can will most likely find an inequality most
	 * quickly by working from right to left. But, we might not be doing
	 * equality testing in which case we must move from left to right.
	 */
	if (is_equal)
		i = ncols - 1;
	else
		i = 0;

	for (j = 0; j < ncols; j++, is_equal ? i-- : i++)
	{
		Datum		attr1,
					attr2;
		bool		isNull1,
					isNull2;
		bool		res;

		attr1 = a[i];
		isNull1 = a_nulls[i];

		attr2 = b[i];
		isNull2 = b_nulls[i];

		if (isNull1 != isNull2)
		{
			if (is_equal)
			{
				result = false; /* one null and one not; they aren't equal */
				break;
			}
			else
			{
				/*
				 * Currently, we assume NULLS LAST. In the future, when we add
				 * support NULLS first to the window order-by clause, we
				 * should take care of NULLS FIRST as well.
				 */
				if (isNull1)
					result = false;
				else
					result = true;
				break;
			}
		}

		else if (isNull1)
			continue;			/* both are null, treat as equal */

		/* Apply the type-specific function */
		res = DatumGetBool(FunctionCall2(&funcs[i], attr1, attr2));

		if (is_equal && !res)
		{
			/* we wanted equality but didn't get it */
			result = false;
			break;
		}
		else if (!is_equal)
		{
			if (!res)
			{
				/*
				 * We wanted it to be less, but it wasn't. Continue only if
				 * they're equal.
				 */
				res = DatumGetBool(FunctionCall2(&eqfuncs[i], attr1, attr2));
				if (res)
				{
					/*
					 * Because we found the previous key equal, set the return
					 * value to false.
					 */
					result = false;
					continue;
				}
				else
					/* LHS is greater than RHS */
					result = false;
			}
			else
			{
				/* because all keys are sorted, we return straight away */
				result = true;
			}
			break;
		}
	}

	MemoryContextSwitchTo(oldContext);

	return result;

}

/*
 * Like heap_deform_tuple(), but we only extract those fields listed in
 * attnums.
 *
 * values and nulls should be allocated memory by the caller.
 */
static void
deform_window_tuple(TupleTableSlot * slot, int nattrs, AttrNumber *attnums,
					Datum *values, bool *nulls, WindowState * wstate)
{
	int			i;
	MemoryContext oldctx = MemoryContextSwitchTo(wstate->transcontext);

	for (i = 0; i < nattrs; i++)
	{
		AttrNumber	attnum = attnums[i];
		bool		isnull;
		Form_pg_attribute attr = slot->tts_tupleDescriptor->attrs[attnum - 1];
		Datum		d = slot_getattr(slot, attnum, &isnull);

		/*
		 * We must copy this tuple because we may need to keep it around a
		 * long time. Caller is responsible for freeing these values.
		 */
		values[i] = datumCopyWithMemManager(0, d,
											attr->attbyval, attr->attlen,
											&(wstate->mem_manager));
		nulls[i] = isnull;
	}

	MemoryContextSwitchTo(oldctx);
}

/*
 * Adjust the trailing and leading edge for ROWS-based framing.
 */
static void
adjustEdgesForRows(WindowFrameBuffer buffer,
				   WindowState * wstate)
{
	WindowStatePerLevel level_state = buffer->level_state;

	if (!(level_state->empty_frame && level_state->has_delay_trail))
	{
		if (EDGE_TRAIL_IS_BOUND(level_state) ||
			EDGE_TRAIL_EQ_CURRENT_ROW(level_state, wstate))
		{
			if (level_state->num_trail_rows <= level_state->trail_rows - 1)
			{
				bool		found = false;
				bool		was_valid = true;

				if (ntuplestore_acc_tell(level_state->trail_reader, NULL))
					found = ntuplestore_acc_advance(level_state->trail_reader, 1);
				else
				{
					was_valid = false;

					if (!EDGE_TRAIL_IS_BOUND_FOLLOWING(level_state) ||
						level_state->trail_rows == 0)
						found = ntuplestore_acc_seek_first(level_state->trail_reader);
				}

				/* Only increment num_trail_rows when we found the next tuple. */
				if (found ||
					(was_valid && level_state->agg_filled &&
					 EDGE_TRAIL_IS_BOUND_FOLLOWING(level_state)))
					level_state->num_trail_rows++;
			}
		}
	}

	if (!(level_state->empty_frame && level_state->has_delay_lead))
	{
		if (EDGE_LEAD_IS_BOUND_PRECEDING(level_state) &&
			level_state->num_lead_rows == level_state->lead_rows)
		{
			ntuplestore_acc_seek_first(level_state->lead_reader);
		}

		else if ((!EDGE_LEAD_IS_BOUND(level_state) &&
				  !EDGE_LEAD_EQ_CURRENT_ROW(level_state, wstate)) ||
				 level_state->num_lead_rows < level_state->lead_rows)
		{
			bool		prev_set = ntuplestore_acc_tell(level_state->lead_reader, NULL);
			bool		found = ntuplestore_acc_advance(level_state->lead_reader, 1);

			if (found || (prev_set && level_state->agg_filled))
				level_state->num_lead_rows++;
		}
	}
}

/*
 * forwardEdgeForRange -- move the edge forward from its previous
 * position in the frame buffer until it points to the key value
 * whose next value is greater than the current edge value for the
 * leading edge, and whose next value is greater than or equal to
 * the current edge value for the trailing edge.
 */
static void
forwardEdgeForRange(WindowStatePerLevel level_state,
					WindowState *wstate,
					ExprState *edge_expr,
					ExprState *edge_range_expr,
					NTupleStoreAccessor * edge_reader,
					WindowEdge edge)
{
	ExprContext *econtext = wstate->ps.ps_ExprContext;
	bool		has_entry = false;
	FrameBufferEntry *entry = level_state->curr_entry_buf;
	Datum		new_edge_value = 0;
	bool		new_edge_value_isnull = true;
	MemoryContext oldctx;

	bool		is_less = true;
	bool		is_equal = true;
	bool		lastrow_edge = false;

	Assert(EDGE_IS_BOUND(level_state, edge));

	if (edge == EDGE_LEAD)
		level_state->lead_ready = false;

	oldctx = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

	/*
	 * Compute the new edge value. Note that this value could be NULL.
	 */
	new_edge_value = ExecEvalExpr(edge_range_expr, econtext,
								  &(new_edge_value_isnull), NULL);

	MemoryContextSwitchTo(oldctx);

	if (!ntuplestore_acc_tell(edge_reader, NULL))
		ntuplestore_acc_seek_first(edge_reader);

	while ((is_equal && edge == EDGE_LEAD) || is_less)
	{
		has_entry = getCurrentValue(edge_reader, level_state, entry);

		if (!has_entry)
			break;

		Assert(level_state->numSortCols <= 1);

		is_less = cmp_deformed_tuple(entry->keys, entry->nulls,
									 &new_edge_value,
									 &new_edge_value_isnull,
									 level_state->numSortCols,
									 level_state->ltfunctions,
									 level_state->eqfunctions,
									 wstate->cmpcontext,
									 false);

		if (!is_less)
		{
			is_equal =
				cmp_deformed_tuple(entry->keys, entry->nulls,
								   &new_edge_value,
								   &new_edge_value_isnull,
								   level_state->numSortCols,
								   level_state->ltfunctions,
								   level_state->eqfunctions,
								   wstate->cmpcontext,
								   true);
		}

		if ((is_equal && edge == EDGE_LEAD) || is_less)
			ntuplestore_acc_advance(edge_reader, 1);
	}

	if (has_entry)
	{
		ntuplestore_acc_advance(edge_reader, -1);
		if (edge == EDGE_LEAD)
			level_state->lead_ready = true;
	}
	else
	{
		lastrow_edge = checkLastRowForEdge(level_state, wstate,
										   edge_reader,
										   new_edge_value,
										   new_edge_value_isnull,
										   edge);
		if (!lastrow_edge || edge == EDGE_TRAIL)
			ntuplestore_acc_seek_last(edge_reader);
	}
}

/*
 * adjustEdgesForRange -- adjust both the trailing and leading edge
 * to the given frame buffer for its level state.
 *
 * This function advances the leading reader until it rests on the position
 * at the frame buffer after which all key values are greater than
 * the leading edge value. The trailing reader rests on the position
 * at the frame buffer which key value is the greatest value in the
 * buffer that is less than the trailing edge value.
 */
static void
adjustEdgesForRange(WindowFrameBuffer buffer,
					WindowState * wstate)
{
	WindowStatePerLevel level_state = buffer->level_state;

	if (!(level_state->empty_frame && level_state->has_delay_trail))
	{
		if (EDGE_TRAIL_EQ_CURRENT_ROW(level_state, wstate))
		{
			NTupleStorePos pos;

			if (ntuplestore_acc_tell(buffer->current_row_reader, &pos))
			{
				ntuplestore_acc_seek(level_state->trail_reader, &pos);
				ntuplestore_acc_advance(level_state->trail_reader, -1);
			}
			else
			{
				ntuplestore_acc_seek_last(level_state->trail_reader);
			}
		}

		else if (EDGE_TRAIL_IS_BOUND(level_state))
		{
			forwardEdgeForRange(level_state, wstate,
								level_state->trail_expr,
								level_state->trail_range_expr,
								level_state->trail_reader,
								EDGE_TRAIL);
		}
	}

	if (!(level_state->empty_frame && level_state->has_delay_lead))
	{
		if (EDGE_LEAD_EQ_CURRENT_ROW(level_state, wstate))
		{
			NTupleStorePos pos;

			if (ntuplestore_acc_tell(buffer->current_row_reader, &pos))
			{
				ntuplestore_acc_seek(level_state->lead_reader, &pos);
				level_state->lead_ready = true;
			}
			else
			{
				ntuplestore_acc_advance(level_state->lead_reader, 1);
				level_state->lead_ready = false;
			}
		}

		else if (EDGE_LEAD_IS_BOUND(level_state))
		{
			forwardEdgeForRange(level_state, wstate,
								level_state->lead_expr,
								level_state->lead_range_expr,
								level_state->lead_reader,
								EDGE_LEAD);
		}

		else
		{
			ntuplestore_acc_advance(level_state->lead_reader, 1);
			level_state->lead_ready = false;
		}
	}
}

/*
 * Adjust the trailing and leading edge to the frame buffer for
 * its list of key levels.
 */
static void
adjustEdges(WindowFrameBuffer buffer,
			WindowState * wstate)
{
	WindowStatePerLevel level_state = buffer->level_state;

	if (EDGE_IS_ROWS(level_state))
		adjustEdgesForRows(buffer, wstate);
	else
		adjustEdgesForRange(buffer, wstate);
}

/*
 * invokeNonTrivialFuncs -- compute transition values for
 * non-trivial window functions.
 */
static void
invokeNonTrivialFuncs(WindowState *wstate)
{
	if (!wstate->trivial_frame)
	{
		WindowStatePerLevel level_state = wstate->level_state;

		computeFrameValue(level_state,
						  wstate,
						  level_state->trail_reader,
						  level_state->lead_reader);
	}
}

static void
finaliseFuncs(WindowState * wstate)
{
	int			funcno;
	ExprContext *econtext = wstate->ps.ps_ExprContext;

	MemoryContext oldctx = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

	/*
	 * Put the current transition value into the econtext
	 */
	for (funcno = 0; funcno < wstate->numfuncs; funcno++)
	{
		WindowStatePerFunction funcstate = &wstate->func_state[funcno];
		FunctionCallInfoData fcinfo;

		/* Evaluate function arguments, save in fcinfo. */

		if (funcstate->plain_agg)
		{
			Datum	   *vals = econtext->ecxt_aggvalues;
			bool	   *isnulls = econtext->ecxt_aggnulls;

			if (OidIsValid(funcstate->finalfn_oid))
			{
				InitFunctionCallInfoData(fcinfo, &(funcstate->finalfn), 1,
										 (void *)wstate, NULL);
				fcinfo.arg[0] = funcstate->final_aggTransValue;
				fcinfo.argnull[0] = funcstate->final_aggTransValueIsNull;
				if (fcinfo.flinfo->fn_strict && funcstate->final_aggTransValueIsNull)
				{
					/* don't call a strict function with NULL inputs */
					vals[funcno] = (Datum)0;
					isnulls[funcno] = true;
				}
				else
				{
					vals[funcno] = FunctionCallInvoke(&fcinfo);
					isnulls[funcno] = fcinfo.isnull;
				}
			}
			else
			{
				vals[funcno] = funcstate->final_aggTransValue;
				isnulls[funcno] = funcstate->final_aggTransValueIsNull;
			}
		}
		else	/* ordinary window function */
		{
			Datum	   *vals = econtext->ecxt_aggvalues;
			bool	   *isnulls = econtext->ecxt_aggnulls;

			vals[funcno] = funcstate->win_value;
			isnulls[funcno] = funcstate->win_value_is_null;
		}
	}

	MemoryContextSwitchTo(oldctx);
}

/*
 * We must evaluate functions in a 'frame aware' way.
 *
 * UNBOUNDED PRECEDING frame edges are managed by passing the transition value
 * from the last call to the function. In this way, we trivially evaluate
 * all rows in the frame which have already passed. It gets more complex
 * for RANGE UNBOUNDED PRECEDING frames because all peers must emit the same
 * value -- so, we remember this in our state.
 *
 * Bounded PRECEDING and FOLLOWING edges are harder. We have two cases.
 *
 * With ROWS bounded frames, we must detect our position from the current
 * rows. If we are earlier on in the tuple count than the frame edge, those
 * tuples must be removed from the the transition value using the inversion
 * method for the function. If one doesn't exist, we do something even more
 * complex. See below. Once in the frame, we only care about the leading
 * frame edge (it could be the current row or some rows from it). So, we must
 * be careful about that.
 *
 * With RANGE bounded frames, we determine the edge with the equality
 * function(s) of the key level. Before and after the frame edges, we do the
 * same as for ROWS frames but remember, we must emit the same value for
 * all peers.
 *
 * If inversion functions do exist for a function, we must evaluate the frame
 * every single time it moves.
 */
static void
invokeWindowFuncs(WindowState * wstate)
{
	bool		found_complex_func = false;

	invokeTrivialFuncs(wstate, &found_complex_func);

	/*
	 * If there are no functions/frames which need non-trivial window
	 * management, we're done.
	 */
	if (found_complex_func)
		invokeNonTrivialFuncs(wstate);

	finaliseFuncs(wstate);
}

/*
 * range_is_negative -- return true if the range expression
 * is negative.
 */
static bool
range_is_negative(ExprState *edge_expr,
				  Datum value)
{
	/*
	 * XXX Most likely not good enough to just compare the value with 0 here.
	 */
	if (value < 0)
		return true;

	return false;
}

/*
 * get_delay_edge -- evaluate the expression from the current row,
 * and return the value for the given DELAY_BOUND edge.
 */
static Datum
get_delay_edge(WindowStatePerLevel level_state,
			   ExprState *edge_expr,
			   WindowState *wstate,
			   WindowEdge edge)
{
	Datum		edge_param = 0;
	bool		isnull = true;
	ExprContext *econtext = wstate->ps.ps_ExprContext;
	MemoryContext oldctx;

	econtext->ecxt_outertuple = wstate->curslot;
	if (TupIsNull(wstate->curslot))
		ereport(ERROR,
				(errcode(ERROR_INVALID_WINDOW_FRAME_PARAMETER),
				 errmsg("%s parameter cannot be NULL",
						(EDGE_IS_ROWS(level_state) ? "ROWS" : "RANGE"))));

	oldctx = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

	edge_param = ExecEvalExpr(edge_expr,
							  econtext,
							  &isnull,
							  NULL);
	MemoryContextSwitchTo(oldctx);

	if (isnull)
		ereport(ERROR,
				(errcode(ERROR_INVALID_WINDOW_FRAME_PARAMETER),
				 errmsg("%s parameter cannot be NULL",
						(EDGE_IS_ROWS(level_state) ? "ROWS" : "RANGE"))));

	if (EDGE_IS_ROWS(level_state) && DatumGetInt64(edge_param) < 0)
		ereport(ERROR,
				(errcode(ERROR_INVALID_WINDOW_FRAME_PARAMETER),
				 errmsg("%s parameter cannot be negative",
						(EDGE_IS_ROWS(level_state) ? "ROWS" : "RANGE"))));

	/* Check if the range expression is negative. */
	if (!EDGE_IS_ROWS(level_state))
	{
		if (range_is_negative(edge_expr, edge_param))
			ereport(ERROR,
					(errcode(ERROR_INVALID_WINDOW_FRAME_PARAMETER),
					 errmsg("%s parameter cannot be negative",
							(EDGE_IS_ROWS(level_state) ? "ROWS" : "RANGE"))));
	}

	if (EDGE_IS_ROWS(level_state) && EDGE_IS_BOUND_PRECEDING(level_state, edge))
		edge_param = Int64GetDatum(0 - DatumGetInt64(edge_param));

	return edge_param;
}

/*
 * checkLastRowForEdge -- check if the last row in the current partition
 * in the input buffer is within frame edges of the current_row.
 *
 * This function is only used for RANGE-frames.
 *
 * This function returns true if the last row is within frame edges
 * of the current_row.
 */
static bool
checkLastRowForEdge(WindowStatePerLevel level_state,
					WindowState *wstate,
					NTupleStoreAccessor * edge_reader,
					Datum new_edge_value,
					bool new_edge_value_isnull,
					WindowEdge edge)
{
	FrameBufferEntry *entry = level_state->curr_entry_buf;
	bool		is_less = true;
	bool		is_equal = true;

	Assert(!EDGE_IS_ROWS(level_state));

	/* Check if the last row is the edge. */
	if (wstate->input_buffer->part_break)
	{
		/*
		 * If the last tuple breaks the partition key, the tuple to check is
		 * the last second tuple in the input buffer.
		 */
		bool		found = ntuplestore_acc_advance(wstate->input_buffer->writer, -1);

		Assert(found);
		ntuplestore_acc_current_tupleslot(wstate->input_buffer->writer,
										  wstate->spare);

		/* Put the writer pointer back to its original place. */
		found = ntuplestore_acc_advance(wstate->input_buffer->writer, 1);
		Assert(found);
	}

	else
	{
		ntuplestore_acc_current_tupleslot(wstate->input_buffer->writer,
										  wstate->spare);
	}

	deform_window_tuple(wstate->spare,
						level_state->numSortCols, level_state->sortColIdx,
						entry->keys, entry->nulls,
						wstate);
	Assert(level_state->numSortCols <= 1);

	is_less = cmp_deformed_tuple(entry->keys, entry->nulls,
								 &new_edge_value, &new_edge_value_isnull,
								 level_state->numSortCols,
								 level_state->ltfunctions,
								 level_state->eqfunctions,
								 wstate->cmpcontext,
								 false);

	if ((EDGE_IS_BOUND_FOLLOWING(level_state, edge) && !is_less) ||
		(EDGE_IS_BOUND_PRECEDING(level_state, edge) && is_less))
	{
		is_equal =
			cmp_deformed_tuple(entry->keys, entry->nulls,
							   &new_edge_value, &new_edge_value_isnull,
							   level_state->numSortCols,
							   level_state->ltfunctions,
							   level_state->eqfunctions,
							   wstate->cmpcontext,
							   true);
	}
	else
		is_equal = false;

	if (is_equal)
		return true;

	if (edge == EDGE_LEAD)
	{
		if ((EDGE_LEAD_IS_BOUND_FOLLOWING(level_state) && is_less) ||
			(EDGE_LEAD_IS_BOUND_PRECEDING(level_state) && is_less))
		{
			return true;
		}
	}
	else
	{
		if ((EDGE_TRAIL_IS_BOUND_FOLLOWING(level_state) && !is_less) ||
			(EDGE_TRAIL_IS_BOUND_PRECEDING(level_state) && !is_less))
			return true;
	}

	return false;
}


/*
 * advanceEdgeForRange -- advance forward/backward a given RANGE-frame
 * edge from its previous position to its new position in the frame
 * buffer.
 */
static void
advanceEdgeForRange(WindowStatePerLevel level_state,
					WindowState * wstate,
					ExprState *edge_expr,
					ExprState *edge_range_expr,
					NTupleStoreAccessor * edge_reader,
					WindowEdge edge)
{
	ExprContext *econtext = wstate->ps.ps_ExprContext;
	bool		has_entry = false;
	FrameBufferEntry *entry = level_state->curr_entry_buf;
	Datum		new_edge_value = 0;
	bool		new_edge_value_isnull = true;

	bool		is_less = false;
	bool		is_equal = false;
	bool		lastrow_edge = false;
	MemoryContext oldctx;

	if (edge == EDGE_LEAD)
		level_state->lead_ready = false;

	oldctx = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

	/*
	 * Compute the new edge value. Note that this value can be NULL.
	 */
	new_edge_value = ExecEvalExpr(edge_range_expr, econtext,
								  &(new_edge_value_isnull), NULL);

	MemoryContextSwitchTo(oldctx);

	if (!ntuplestore_acc_tell(edge_reader, NULL))
	{
		lastrow_edge = checkLastRowForEdge(level_state, wstate,
										   edge_reader,
										   new_edge_value,
										   new_edge_value_isnull,
										   edge);

		if ((edge == EDGE_LEAD || lastrow_edge) && EDGE_IS_BOUND_PRECEDING(level_state, edge))
			ntuplestore_acc_seek_last(edge_reader);
	}

	if (!EDGE_EQ_CURRENT_ROW(level_state, wstate, edge))
	{
		bool		prev_is_equal = false;

		do
		{
			has_entry = getCurrentValue(edge_reader, level_state, entry);

			if (!has_entry)
				break;

			Assert(level_state->numSortCols <= 1);

			is_less = cmp_deformed_tuple(entry->keys, entry->nulls,
										 &new_edge_value,
										 &new_edge_value_isnull,
										 level_state->numSortCols,
										 level_state->ltfunctions,
										 level_state->eqfunctions,
										 wstate->cmpcontext,
										 false);
			prev_is_equal = is_equal;
			is_equal =
				cmp_deformed_tuple(entry->keys, entry->nulls,
								   &new_edge_value,
								   &new_edge_value_isnull,
								   level_state->numSortCols,
								   level_state->ltfunctions,
								   level_state->eqfunctions,
								   wstate->cmpcontext,
								   true);

			if ((EDGE_IS_BOUND_FOLLOWING(level_state, edge) && (is_less || is_equal)) ||
				(EDGE_IS_BOUND_PRECEDING(level_state, edge) && (!is_less || is_equal)))
			{
				if (EDGE_IS_BOUND_FOLLOWING(level_state, edge))
					ntuplestore_acc_advance(edge_reader, 1);
				else
					ntuplestore_acc_advance(edge_reader, -1);
			}

		} while ((EDGE_IS_BOUND_FOLLOWING(level_state, edge) && (is_less || is_equal)) ||
				 (EDGE_IS_BOUND_PRECEDING(level_state, edge) && (!is_less || is_equal)));

		if (has_entry)
		{
			if (EDGE_IS_BOUND_FOLLOWING(level_state, edge))
			{
				/*
				 * When the edge is following, edge_reader points to the
				 * position whose value is after the new edge value. We need
				 * to back off one position if the value in its previous
				 * position is equal to the new edge value, or this is the
				 * leading edge.
				 */
				if (prev_is_equal || edge == EDGE_LEAD)
					ntuplestore_acc_advance(edge_reader, -1);
			}

			else
			{
				/*
				 * When the edge is preceding, edge_reader points to the
				 * position whose value is before the new edge value. We need
				 * to back off one if the value in its next position is equal
				 * to the new edge value. We also back off one position if
				 * this edge is the trailing edge.
				 */
				if (prev_is_equal || edge == EDGE_TRAIL)
					ntuplestore_acc_advance(edge_reader, 1);
			}

			if (edge == EDGE_LEAD)
				level_state->lead_ready = true;
		}

		else
		{
			ntuplestore_acc_set_invalid(edge_reader);

			if (!EDGE_IS_BOUND_FOLLOWING(level_state, edge))
			{
				/*
				 * When the leading edge is "x preceding", check the first
				 * value in the buffer. If this value is equal to the new edge
				 * value, point the lead_reader to this value.
				 */
				if (edge == EDGE_LEAD && is_equal)
					ntuplestore_acc_seek_first(edge_reader);

				return;
			}

			Assert(EDGE_IS_BOUND_FOLLOWING(level_state, edge));

			/*
			 * If the last value in the frame buffer is equal to the new edge
			 * value, simple set the reader to the last value. Otherwise, we
			 * check with the last row in the input buffer.
			 */
			if (is_equal)
				ntuplestore_acc_seek_last(edge_reader);
			else
			{
				lastrow_edge = checkLastRowForEdge(level_state, wstate,
												   edge_reader,
												   new_edge_value,
												   new_edge_value_isnull,
												   edge);
				if (!lastrow_edge && edge == EDGE_LEAD)
					ntuplestore_acc_seek_last(edge_reader);
			}
		}
	}

	/* For the trailing edge, we also back off one. */
	if (edge == EDGE_TRAIL)
	{
		bool		found = false;

		found = ntuplestore_acc_advance(edge_reader, -1);

		if (!found)
		{
			if (EDGE_TRAIL_EQ_CURRENT_ROW(level_state, wstate))
			{
				if (!ntuplestore_acc_tell(level_state->frame_buffer->current_row_reader,
										  NULL))
					ntuplestore_acc_seek_last(edge_reader);
			}
			else if (EDGE_TRAIL_IS_BOUND_FOLLOWING(level_state))
			{
				lastrow_edge = checkLastRowForEdge(level_state, wstate,
												   edge_reader,
												   new_edge_value,
												   new_edge_value_isnull,
												   edge);
				if (lastrow_edge)
					ntuplestore_acc_seek_last(edge_reader);
			}
		}
	}
}

/*
 * adjustDelayBoundEdgeForRange -- evaluate the given DELAY_BOUND edge
 * in RANGE-type frames and adjust the corresponding edge to the frame
 * buffer based on this new value.
 */
static void
adjustDelayBoundEdgeForRange(WindowStatePerLevel level_state,
							 WindowState * wstate,
							 ExprState *edge_expr,
							 ExprState *edge_range_expr,
							 NTupleStoreAccessor * edge_reader,
							 Datum *p_request_value,
							 WindowEdge edge)
{
	WindowFrameBuffer frame_buffer = level_state->frame_buffer;
	NTupleStorePos pos;
	bool		found;

	Assert(level_state->has_delay_bound);
	Assert(!EDGE_IS_ROWS(level_state));
	Assert(level_state->numSortCols == 1);

	/* Compute the new edge value */
	*p_request_value =
		get_delay_edge(level_state, edge_expr, wstate, edge);

	/* Set the edge to current_row */
	found = ntuplestore_acc_tell(frame_buffer->current_row_reader, &pos);
	if (found)
	{
		found = ntuplestore_acc_seek(edge_reader, &pos);
		Assert(found);
	}
	else
	{
		ntuplestore_acc_set_invalid(edge_reader);
	}

	advanceEdgeForRange(level_state, wstate,
						edge_expr, edge_range_expr,
						edge_reader, edge);
}

/*
 * adjustDelayBoundEdgeForRows -- evaluate the given DELAY_BOUND edge in
 * a ROWS-type frame and adjust the corresponding edge to the frame buffer
 * based on this new edge value.
 */
static void
adjustDelayBoundEdgeForRows(WindowStatePerLevel level_state,
							WindowState * wstate,
							ExprState *edge_expr,
							NTupleStoreAccessor * edge_reader,
							int64 *p_request_num_rows,
							int64 *p_num_rows,
							WindowEdge edge)
{
	WindowFrameBuffer frame_buffer = level_state->frame_buffer;
	bool		found;

	Assert(level_state->has_delay_bound);
	Assert(EDGE_IS_ROWS(level_state));
	Assert(list_length(level_state->level_funcs) >= 1);

	*p_request_num_rows =
		get_delay_edge(level_state, edge_expr, wstate, edge);

	*p_num_rows = 0;

	/* Reset the edge_reader position in the frame buffer. */
	if (EDGE_IS_BOUND_FOLLOWING(level_state, edge))
	{
		NTupleStorePos curr_row_pos;

		if (ntuplestore_acc_tell(frame_buffer->current_row_reader, &curr_row_pos))
		{
			/*
			 * Set edge_reader to current_row and advance it to
			 * *p_request_num_rows.
			 */
			found = ntuplestore_acc_seek(edge_reader, &curr_row_pos);
			Assert(found);

			if (frame_buffer->num_rows_after - 1 >= *p_request_num_rows)
			{
				found = ntuplestore_acc_advance(edge_reader, *p_request_num_rows);
				Assert(found);
				*p_num_rows = *p_request_num_rows;
			}
			else
			{
				ntuplestore_acc_set_invalid(edge_reader);
				*p_num_rows = frame_buffer->num_rows_after - 1;
				if (level_state->agg_filled)
					(*p_num_rows)++;
			}
		}

		else
		{
			ntuplestore_acc_set_invalid(edge_reader);
			*p_num_rows = 0;
		}
	}

	else
	{
		NTupleStorePos curr_row_pos;
		int			ntuples = *p_request_num_rows;

		*p_num_rows = 0;

		if (ntuplestore_acc_tell(frame_buffer->current_row_reader, &curr_row_pos))
		{
			found = ntuplestore_acc_seek(edge_reader, &curr_row_pos);
			Assert(found);

		}
		else
		{
			found = ntuplestore_acc_seek_last(edge_reader);
			found = ntuplestore_acc_tell(edge_reader, &curr_row_pos);
			if (level_state->agg_filled)
				ntuples++;
		}

		if (found)
		{
			if (frame_buffer->num_rows_before >= 0 - *p_request_num_rows)
			{
				found = ntuplestore_acc_advance(edge_reader, ntuples);
				*p_num_rows = *p_request_num_rows;
			}
			else
			{
				ntuplestore_acc_set_invalid(edge_reader);

				*p_num_rows = 0 - frame_buffer->num_rows_before;
			}
		}
	}

	/* For the trailing edge, we also back off one. */
	if (edge == EDGE_TRAIL)
	{
		/*
		 * If the frame is "0 preceding/following" and the current_row_reader
		 * is pointing to an invalid position, we simple set the trailing edge
		 * to the last entry in the frame buffer.
		 */
		if (*p_request_num_rows == 0 &&
			!ntuplestore_acc_tell(frame_buffer->current_row_reader, NULL))
		{
			ntuplestore_acc_seek_last(edge_reader);
		}

		/*
		 * If the trailing edge points to an invalid position and the edge is
		 * "x following", we want to set the current trailing edge to the end
		 * of the buffer.
		 */
		else if (*p_num_rows > 0 &&
				 !ntuplestore_acc_tell(edge_reader, NULL))
		{
			ntuplestore_acc_seek_last(edge_reader);
			(*p_num_rows)--;
		}

		else
			ntuplestore_acc_advance(edge_reader, -1);
	}
}

/*
 * fetchCurrentRow -- retrieve the current_row from the input buffer.
 *
 * The current_row_reader always points to the previous current_row
 * position. When there is no rows in the buffer, this function reads
 * the first tuple from the outer plan.
 */
static TupleTableSlot *
fetchCurrentRow(WindowState * wstate)
{
	bool		found = false;
	WindowInputBuffer buffer = wstate->input_buffer;
	WindowAgg  *window = (WindowAgg *) wstate->ps.plan;
	bool		has_prior_tuple = false;
	ExprContext *econtext = wstate->ps.ps_ExprContext;

	if (buffer != NULL)
	{
		/* read the previous slot */
		ntuplestore_acc_current_tupleslot(buffer->current_row_reader,
										  wstate->priorslot);
		has_prior_tuple = true;

		found = ntuplestore_acc_advance(buffer->current_row_reader, 1);
	}

	if (found)
		wstate->cur_slot_is_new = false;
	else
	{
		if (wstate->is_input_done)
			return NULL;

		/* Fetch the first tuple from the outer plan */
		TupleTableSlot *slot = ExecProcNode(outerPlanState(wstate));

		if (TupIsNull(slot))
			return NULL;

		if (buffer == NULL)
		{
			initializePartition(wstate);
		}

		ntuplestore_acc_put_tupleslot(wstate->input_buffer->writer, slot);
		wstate->input_buffer->num_tuples++;

		if (buffer == NULL)
		{
			/* The first tuple will not break any keys. */
			wstate->cur_slot_key_break = wstate->trivial_frame ? 0 : 1;
			ntuplestore_acc_seek_first(wstate->input_buffer->current_row_reader);
			ntuplestore_acc_seek_first(wstate->input_buffer->reader);
		}

		else
		{
			ntuplestore_acc_seek_last(wstate->input_buffer->current_row_reader);
			ntuplestore_acc_seek_last(wstate->input_buffer->reader);
		}

		buffer = wstate->input_buffer;

		if (wstate->curslot->tts_tupleDescriptor == NULL)
			ExecSetSlotDescriptor(wstate->curslot,
								  slot->tts_tupleDescriptor);
		if (wstate->priorslot->tts_tupleDescriptor == NULL)
			ExecSetSlotDescriptor(wstate->priorslot,
								  slot->tts_tupleDescriptor);
		if (wstate->spare->tts_tupleDescriptor == NULL)
			ExecSetSlotDescriptor(wstate->spare,
								  slot->tts_tupleDescriptor);

		wstate->cur_slot_is_new = true;
	}

	found = ntuplestore_acc_current_tupleslot(buffer->current_row_reader,
											  wstate->curslot);
	Assert(found);

	wstate->cur_slot_part_break = false;
	wstate->cur_slot_key_break = -1;

	/* Check if the partition key breaks */
	if (has_prior_tuple)
	{
		wstate->cur_slot_part_break =
			(window->partNumCols &&
			 (!execTuplesMatch(wstate->curslot, wstate->priorslot,
							   window->partNumCols, window->partColIdx,
							   wstate->eqfunctions,
							   econtext->ecxt_per_tuple_memory)));
		if (wstate->cur_slot_part_break)
			wstate->input_buffer->part_break = true;
		else
		{
			bool		match = true;

			/*
			 * Process ordering partial key that breaks. We also increment the
			 * peer_count.
			 */
			if (!wstate->trivial_frame)
			{
				WindowStatePerLevel lvl = wstate->level_state;

				match = execTuplesMatch(wstate->curslot, wstate->priorslot,
										lvl->numSortCols,
										lvl->sortColIdx,
										lvl->eqfunctions,
										econtext->ecxt_per_tuple_memory);
				if (!match)
					wstate->cur_slot_key_break = 0;
				else
					lvl->peer_index++;
			}
		}
	}

	wstate->row_index++;
	if (wstate->cur_slot_key_break == 0 &&
		!wstate->trivial_frame)
		advanceKeyLevelState(wstate);

	/*
	 * If this tuple breaks the partition key, re-initialize the state.
	 */
	if (wstate->cur_slot_part_break)
		initializePartition(wstate);
	else
		trimInputBuffer(buffer);

	/*
	 * Evaluate the DELAY_BOUND edges, including LEAD/LAG offset expressions,
	 * and change the framing clauses accordingly.
	 *
	 * We also adjust the trailing edge and the leading edge to the frame buffer
	 * based on these new edge values.
	 */
	if (!wstate->trivial_frame)
	{
		WindowStatePerLevel level_state = wstate->level_state;

		if (level_state->has_delay_lead)
		{
			if (EDGE_IS_ROWS(level_state))
				adjustDelayBoundEdgeForRows(level_state, wstate,
											level_state->lead_expr,
											level_state->lead_reader,
											&(level_state->lead_rows),
											&(level_state->num_lead_rows),
											EDGE_LEAD);
			else
				adjustDelayBoundEdgeForRange(level_state, wstate,
											 level_state->lead_expr,
											 level_state->lead_range_expr,
											 level_state->lead_reader,
											 &(level_state->lead_range),
											 EDGE_LEAD);
		}

		if (level_state->has_delay_trail)
		{
			if (EDGE_IS_ROWS(level_state))
			{
				adjustDelayBoundEdgeForRows(level_state, wstate,
											level_state->trail_expr,
											level_state->trail_reader,
											&(level_state->trail_rows),
											&(level_state->num_trail_rows),
											EDGE_TRAIL);
			}
			else
			{
				adjustDelayBoundEdgeForRange(level_state, wstate,
											 level_state->trail_expr,
											 level_state->trail_range_expr,
											 level_state->trail_reader,
											 &(level_state->trail_range),
											 EDGE_TRAIL);
			}
		}

		/* set empty_frame flag */
		setEmptyFrame(level_state, wstate);
	}

	return wstate->curslot;
}

/*
 * fetchTupleSlotThroughBuf -- fetch a tuple through the reader of the
 * input buffer.
 */
static TupleTableSlot *
fetchTupleSlotThroughBuf(WindowState * wstate)
{
	WindowInputBuffer buffer = wstate->input_buffer;
	bool		found = false;
	WindowAgg  *window = (WindowAgg *) wstate->ps.plan;
	ExprContext *econtext = wstate->ps.ps_ExprContext;
	bool		has_prior_tuple = false;

	/* Read the previous tupleslot if any */
	if (ntuplestore_acc_tell(buffer->reader, NULL))
	{
		found = ntuplestore_acc_current_tupleslot(buffer->reader,
												  wstate->priorslot);
		has_prior_tuple = found;
	}

	if (found)
	{
		/* Advance the reader */
		found = ntuplestore_acc_advance(buffer->reader, 1);
		Assert(!found);
	}


	if (!found)
	{
		TupleTableSlot *slot = ExecProcNode(outerPlanState(wstate));

		if (TupIsNull(slot))
		{
			wstate->is_input_done = true;
			return NULL;
		}

		/* Put the new tuple into the input buffer */
		ntuplestore_acc_put_tupleslot(buffer->writer, slot);
		buffer->num_tuples++;

		ntuplestore_acc_seek_last(buffer->reader);

		ntuplestore_acc_current_tupleslot(buffer->writer,
										  wstate->spare);
		wstate->cur_slot_is_new = true;
	}

	wstate->cur_slot_part_break = false;
	wstate->cur_slot_key_break = -1;

	if (!TupIsNull(wstate->priorslot))
	{
		wstate->cur_slot_part_break =
			(window->partNumCols &&
			 (!execTuplesMatch(wstate->spare, wstate->priorslot,
							   window->partNumCols, window->partColIdx,
							   wstate->eqfunctions,
							   econtext->ecxt_per_tuple_memory)));

		if (wstate->cur_slot_part_break)
			buffer->part_break = true;

		else
		{
			bool		match;

			/*
			 * Process ordering partial key that breaks. We also increment the
			 * peer_index.
			 */
			if (!wstate->trivial_frame)
			{
				WindowStatePerLevel lvl = wstate->level_state;

				match = execTuplesMatch(wstate->spare, wstate->priorslot,
										lvl->numSortCols,
										lvl->sortColIdx,
										lvl->eqfunctions,
										econtext->ecxt_per_tuple_memory);
				if (!match)
					wstate->cur_slot_key_break = 0;
				else
					lvl->peer_count++;
			}
		}
	}

	return wstate->spare;
}

/*
 * ExecWindow
 *
 */
TupleTableSlot *
ExecWindow(WindowState * wstate)
{
	bool		output_ready;
	TupleTableSlot *next_tupleslot;
	TupleTableSlot *resultSlot;
	ExprContext *econtext;
	ExprDoneCond isDone;
	bool		last_peer = false;

	econtext = wstate->ps.ps_ExprContext;

	/* Fetch the current_row */
	econtext->ecxt_outertuple = fetchCurrentRow(wstate);

	if (TupIsNull(econtext->ecxt_outertuple))
	{
		ExecEagerFreeWindow(wstate);

		return NULL;			/* we are done */
	}

	/* Process the current_row if it has not been processed. */
	if (wstate->cur_slot_is_new)
	{
		processTupleSlot(wstate, wstate->curslot, false);
		wstate->cur_slot_is_new = false;
	}

	/*
	 * Increment the current_row in the frame buffer for each key level.
	 */
	if (!wstate->trivial_frame)
	{
		WindowStatePerLevel level_state = wstate->level_state;

		if (level_state->empty_frame &&
			!level_state->has_delay_bound)
		{
			/* do nothing */
		}
		else if (!level_state->trivial_frames_only)
		{
			if ((wstate->input_buffer->num_tuples > 1) &&
				(EDGE_IS_ROWS(level_state) ||
				 (wstate->cur_slot_key_break != -1 &&
				  0 >= wstate->cur_slot_key_break)))
			{
				econtext->ecxt_outertuple = wstate->curslot;
				incrementCurrentRow(level_state->frame_buffer, wstate);
			}
		}
	}

	output_ready = checkOutputReady(wstate);

	while (!output_ready)
	{
		next_tupleslot = fetchTupleSlotThroughBuf(wstate);

		if (TupIsNull(next_tupleslot) || wstate->cur_slot_part_break)
			last_peer = true;

		processTupleSlot(wstate, next_tupleslot, last_peer);

		if (!TupIsNull(next_tupleslot))
			output_ready = checkOutputReady(wstate);
		else
			output_ready = true;
	}

	ResetExprContext(econtext);

	econtext->ecxt_outertuple = wstate->curslot;
	econtext->ecxt_outertuple = wstate->curslot;

	invokeWindowFuncs(wstate);

	/*
	 * Form the result tuple using ExecProject(), and return it.
	 */
	resultSlot = ExecProject(wstate->ps.ps_ProjInfo, &isDone);

	if (TupIsNull(resultSlot))
		ExecEagerFreeWindow(wstate);

	return resultSlot;
}

/*
 * resetTransValue -- reset the transition value for
 * a given function.
 */
static void
resetTransValue(WindowStatePerFunction funcstate,
				WindowState * wstate)
{
	if (funcstate->plain_agg)
	{
		freeTransValue(&funcstate->aggTransValue,
					   funcstate->aggTranstypeByVal,
					   &funcstate->aggTransValueIsNull,
					   &funcstate->aggNoTransValue,
					   true);

		if (!funcstate->aggTranstypeByVal && !funcstate->aggTransValueIsNull &&
			DatumGetPointer(funcstate->aggTransValue) != NULL)
			pfree(DatumGetPointer(funcstate->aggTransValue));

		if (funcstate->aggInitValueIsNull)
			funcstate->aggTransValue = funcstate->aggInitValue;
		else
		{
			funcstate->aggTransValue =
				datumCopyWithMemManager(0, funcstate->aggInitValue,
										funcstate->aggTranstypeByVal,
										funcstate->aggTranstypeLen,
										&(wstate->mem_manager));
		}

		funcstate->aggTransValueIsNull = funcstate->aggInitValueIsNull;
		funcstate->aggNoTransValue = funcstate->aggInitValueIsNull;
	}
	else
	{
		int			argno = 1;
		WindowFuncExprState *wfxstate = funcstate->wfuncstate;

		if (wfxstate->argtypbyval)
			freeTransValue(&funcstate->aggTransValue,
						   wfxstate->argtypbyval[argno],
						   &funcstate->aggTransValueIsNull,
						   &funcstate->aggNoTransValue,
						   true);

		funcstate->win_value = 0;
		funcstate->win_value_is_null = true;
	}
}


/*
 * resetTransValues -- reset the transition values for
 * each function.
 */
static void
resetTransValues(WindowStatePerLevel level_state,
				 WindowState * wstate)
{
	ListCell   *lc;

	foreach(lc, level_state->level_funcs)
	{
		WindowStatePerFunction funcstate = (WindowStatePerFunction) lfirst(lc);

		resetTransValue(funcstate, wstate);
	}
}


/*
 * processTupleSlot -- process a new tuple.
 *
 * This function passes the given tuple to each function in every
 * key level to compute an intermediate result.
 *
 * We only handle non-trivial window functions here.
 *
 * 'last_peer' indicates if the tuple to be inserted is the last tuple from
 * the input. It is used to determine if the trailing edge and leading edge
 * need to be adjusted.
 */
static void
processTupleSlot(WindowState * wstate, TupleTableSlot * slot, bool last_peer)
{
	ExprContext *econtext = wstate->ps.ps_ExprContext;
	WindowStatePerLevel level_state = wstate->level_state;
	ListCell   *lc;
	bool		adv_peercount = false;

	if (wstate->trivial_frame)
		return;

	/*
	 * For non-delayed frames, we can simply ignore this level. However,
	 * for delayed frames, we still need to store the intermediate results
	 * because they may be needed later.
	 */
	if (level_state->empty_frame &&
		!level_state->has_delay_bound)
	{
		/* do nothing */
		return;
	}

	if (level_state->trivial_frames_only)
		return;

	bool		write_pre_value = false;

	if (wstate->input_buffer->num_tuples > 1 &&
		wstate->cur_slot_is_new)
	{
		if (EDGE_IS_ROWS(level_state) || level_state->has_only_trans_funcs)
			write_pre_value = true;
		else if (wstate->cur_slot_part_break || TupIsNull(slot))
			write_pre_value = true;
		else if ((wstate->cur_slot_key_break != -1 &&
				  0 >= wstate->cur_slot_key_break))
			write_pre_value = true;
	}

	/*
	 * If this tuple breaks the order by key in this level, we write
	 * out previous aggregate values if any.
	 */
	if (write_pre_value)
	{
		/*
		 * If there is a function that requires peer counts, we need
		 * to compute its preliminary value before appending it to the
		 * frame buffer.
		 */
		if (level_state->need_peercount)
		{
			foreach(lc, level_state->level_funcs)
			{
				WindowStatePerFunction funcstate =
					(WindowStatePerFunction) lfirst(lc);

				if (funcstate->winpeercount)
					add_tuple_to_trans(funcstate, wstate, econtext, false);
			}
		}

		/*
		 * Set econtext->ecxt_outertuple because the range frame needs
		 * this for order keys.
		 */
		econtext->ecxt_outertuple = wstate->priorslot;
		appendToFrameBuffer(level_state, wstate, last_peer);

		/*
		 * Reset the transition value for those functions which has
		 * preliminary functions, but not inverse preliminary
		 * functions.
		 */
		foreach(lc, level_state->level_funcs)
		{
			WindowStatePerFunction funcstate = (WindowStatePerFunction) lfirst(lc);

			if (funcstate->plain_agg &&
				!funcstate->cumul_frame &&
				!OidIsValid(funcstate->invprelimfn_oid) &&
				OidIsValid(funcstate->prelimfn_oid))
			{
				resetTransValue(funcstate, wstate);
			}
			else if ((IS_LEAD_LAG(funcstate->wfuncstate->winkind) ||
					  IS_FIRST_LAST(funcstate->wfuncstate->winkind)) &&
					 !last_peer)
			{
				resetTransValue(funcstate, wstate);
			}
		}
	}

	/*
	 * If this slot causes the partition key to break, we don't add
	 * this tuple to the transition value.
	 */
	if (wstate->cur_slot_part_break || TupIsNull(slot))
	{
		if (write_pre_value)
			level_state->agg_filled = false;

		return;
	}

	foreach(lc, level_state->level_funcs)
	{
		WindowStatePerFunction funcstate =
			(WindowStatePerFunction) lfirst(lc);
		ExprContext *econtext = wstate->ps.ps_ExprContext;

		if (funcstate->trivial_frame)
			continue;

		if (HAS_ONLY_TRANS_FUNC(funcstate))
			continue;

		/*
		 * Increment the values for peer counts. There may be more
		 * than one such function in one key level. We should catch
		 * this in the earlier stage, but in case we didn't, we only
		 * do this once.
		 */
		if (!adv_peercount && funcstate->winpeercount)
		{
			if (wstate->cur_slot_is_new &&
				wstate->cur_slot_key_break == 0)
			{
				advancePeerCount(level_state);
			}

			adv_peercount = true;
		}
		else
		{
			/* Add this tuple to its transition value */
			econtext->ecxt_outertuple = slot;

			add_tuple_to_trans(funcstate, wstate, econtext, true);

			level_state->agg_filled = true;
		}
	}
}


/*
 * checkOutputReady -- check each key level to see if its frame buffer
 * has sufficient data to compute one output rows. If so, return true,
 * othwise, return false.
 */
static bool
checkOutputReady(WindowState * wstate)
{
	ExprContext *econtext = wstate->ps.ps_ExprContext;

	econtext->ecxt_outertuple = wstate->curslot;

	if (wstate->input_buffer->part_break)
		return true;

	if (!wstate->trivial_frame)
	{
		WindowStatePerLevel level_state = wstate->level_state;

		if (level_state->trivial_frames_only)
			return true;

		if (EDGE_IS_ROWS(level_state))
		{
			if (!hasEnoughDataInRows(level_state->frame_buffer,
									 level_state,
									 wstate,
									 level_state->trail_rows,
									 level_state->lead_rows))
				return false;
		}

		else
		{
			if (!hasEnoughDataInRange(level_state->frame_buffer,
									  level_state,
									  wstate,
									  level_state->trail_range,
									  level_state->lead_range))
				return false;
		}
	}

	return true;
}

/*
 * coerceType -- coerce the frame parameter expression to a given type.
 */
static Expr *
coerceType(Expr *expr, Oid new_type)
{
	Oid			expr_type = 0;
	int32		expr_typmod = 0;
	Expr	   *new_expr = expr;

	expr_type = exprType((Node *) expr);
	expr_typmod = exprTypmod((Node *) expr);

	if (expr_type != new_type)
	{
		new_expr = (Expr *)coerce_to_target_type(NULL,
												 (Node *) expr,
												 expr_type,
												 new_type, expr_typmod,
												 COERCION_EXPLICIT,
												 COERCE_IMPLICIT_CAST,
												 -1);
	}

	return new_expr;
}

static void
init_bound_frame_edge_expr(WindowStatePerLevel level_state,
						   TupleDesc desc,
						   AttrNumber attnum,
						   WindowEdge edge,
						   WindowState * wstate)
{
	ExprState  *n;
	Expr	   *orig_expr;
	Expr	   *expr;
	Expr	   *varexpr;
	Oid			exprrestype;
	Oid			ltype ,
				rtype;
	HeapTuple	tup;
	ExprContext *econtext = wstate->ps.ps_ExprContext;
	bool		isNull;
	char	   *oprname;
	Form_pg_operator opr;
	int32		vartypmod = desc->attrs[attnum - 1]->atttypmod;

	Insist(EDGE_IS_BOUND(level_state, edge));

	if (edge == EDGE_LEAD)
		orig_expr = (Expr *) level_state->endOffset;
	else
		orig_expr = (Expr *) level_state->startOffset;
	expr = orig_expr;

	ltype = desc->attrs[attnum - 1]->atttypid;
	rtype = exprType((Node *) orig_expr);

	varexpr = (Expr *) makeVar(OUTER, attnum, ltype, vartypmod, 0);

	/*
	 * Set trailing or leading expression state. If one of such expression
	 * exist, we coerce the other one to the same type.
	 */
	if (edge == EDGE_TRAIL)
	{
		if (level_state->lead_expr != NULL)
		{
			expr = coerceType(expr, exprType((Node *) (level_state->lead_expr->expr)));
			if (expr == NULL)
				ereport(ERROR,
						(errcode(ERRCODE_DATA_EXCEPTION),
						 errmsg("can't coerce trailing frame bound to type of leading frame bound"),
						 errhint("specify the leading and trailing frame bounds as the same type")));
		}

		n = ExecInitExpr(expr, (PlanState *) wstate);

		level_state->trail_expr = n;
		if (!level_state->has_delay_trail)
			level_state->trail_range = ExecEvalExpr(n, econtext, &isNull, NULL);
	}
	else
	{
		if (level_state->trail_expr != NULL)
		{
			expr = coerceType(expr, exprType((Node *) (level_state->trail_expr->expr)));
			if (expr == NULL)
				ereport(ERROR,
						(errcode(ERRCODE_DATA_EXCEPTION),
						 errmsg("can't coerce leading frame bound to type of trailing frame bound"),
						 errhint("specify the leading and trailing frame bounds as the same type")));
		}

		n = ExecInitExpr((Expr *)expr, (PlanState *) wstate);

		level_state->lead_expr = n;
		if (!level_state->has_delay_lead)
			level_state->lead_range = ExecEvalExpr(n, econtext, &isNull, NULL);
	}

	if ((EDGE_IS_BOUND_PRECEDING(level_state, edge) &&
		 level_state->col_sort_asc[0]) ||
		(EDGE_IS_BOUND_FOLLOWING(level_state, edge) &&
		 !level_state->col_sort_asc[0]))
	{
		oprname = "-";
	}
	else
	{
		oprname = "+";
	}

	tup = SearchSysCache4(OPERNAMENSP,
						  CStringGetDatum(oprname),
						  ObjectIdGetDatum(ltype),
						  ObjectIdGetDatum(rtype),
						  ObjectIdGetDatum(PG_CATALOG_NAMESPACE));

	/*
	 * If we didn't find an operator, it's probably because the user has
	 * specified a RANGE parameter which does not have an implicit cast to the
	 * sorting column. So, see what operator the parser found via oper() and
	 * arrange for the LHS to be cast to that for the purpose of the
	 * expression evaluation.
	 */
	if (!HeapTupleIsValid(tup))
	{
		HeapTuple	tup2;

		List	   *oprlist = lappend(NIL, makeString(oprname));

		tup2 = oper(NULL, oprlist, ltype, rtype, true, -1);

		list_free_deep(oprlist);

		opr = (Form_pg_operator)GETSTRUCT(tup2);

		/* this is why we're here */
		Insist(ltype != opr->oprleft);

		varexpr = (Expr *)coerce_to_target_type(NULL, (Node *) varexpr,
											  ltype, opr->oprleft, vartypmod,
												COERCION_EXPLICIT,
												COERCE_IMPLICIT_CAST,
												-1);

		exprrestype = opr->oprresult;
		expr = make_opclause(HeapTupleGetOid(tup2), exprrestype,
							 false, varexpr,
							 orig_expr);
		((OpExpr *) expr)->opfuncid = opr->oprcode;
		ReleaseSysCache(tup2);
	}
	else
	{
		opr = ((Form_pg_operator)GETSTRUCT(tup));

		exprrestype = opr->oprresult;
		expr = make_opclause(HeapTupleGetOid(tup), exprrestype,
							 false, varexpr,
							 orig_expr);
		((OpExpr *) expr)->opfuncid = opr->oprcode;
		ReleaseSysCache(tup);
	}

	/*
	 * If the frame edge operation returns a different type to the input type,
	 * we must coerce it back.
	 */
	if (ltype != exprrestype)
	{
		expr =
			(Expr *)coerce_to_target_type(NULL,
										  (Node *) expr,
										  exprrestype,
										  ltype, vartypmod,
										  COERCION_EXPLICIT,
										  COERCE_IMPLICIT_CAST,
										  -1);
	}

	Insist(PointerIsValid(expr));

	n = ExecInitExpr(expr, (PlanState *) wstate);

	if (edge == EDGE_TRAIL)
		level_state->trail_range_expr = n;
	else
		level_state->lead_range_expr = n;

	/*
	 * Construct immediate clause like: var +/- range = var where var is ORDER
	 * BY value and range is frame range value, so "var +/- range" means frame
	 * edge created above (expr). Now, if the left hand equals to the right
	 * hand, the frame edge must be on the current row. The created expression
	 * is used to determine if the frame edge is on the current row or not.
	 */
	n = make_eq_exprstate(wstate, expr, varexpr);
	if (edge == EDGE_TRAIL)
		level_state->trail_range_eq_expr = n;
	else
		level_state->lead_range_eq_expr = n;
}

/*
 * make_eq_exprstate
 * Given expr1 and expr2, make clase "expr1 = expr2".
 * In case the result types of expr1 and expr2 are different,
 * the cast is installed on expr2.
 */
static ExprState *
make_eq_exprstate(WindowState * wstate, Expr *expr1, Expr *expr2)
{
	Oid			restype1,
				restype2;
	Expr	   *eq_expr;
	Operator	eq_optup;
	Oid			eq_opid;

	restype1 = exprType((Node *) expr1);
	restype2 = exprType((Node *) expr2);
	if (restype1 != restype2)
	{
		expr2 = (Expr *)coerce_to_target_type(NULL,
											  (Node *) expr2,
											  restype2,
											  restype1,
											  exprTypmod((Node *) expr2),
											  COERCION_EXPLICIT,
											  COERCE_IMPLICIT_CAST,
											  -1);
	}

	eq_optup = equality_oper(restype1, false);
	Assert(exprType((Node *) expr1) == exprType((Node *) expr2));
	eq_opid = oprid(eq_optup);
	eq_expr = make_opclause(eq_opid, BOOLOID, false, expr1, expr2);
	((OpExpr *) eq_expr)->opfuncid = oprfuncid(eq_optup);
	ReleaseSysCache(eq_optup);

	return ExecInitExpr(eq_expr, (PlanState *) wstate);
}

/*
 * exec_eq_exprstate
 * Executes eq_exprstate made in make_eq_exprstate() and returns
 * bool result. It sets ecxt_outertuple to the current slot
 * so that Vars contained in eq_exprstate point to the current row.
 */
static bool
exec_eq_exprstate(WindowState * wstate, ExprState *eq_exprstate)
{
	ExprContext *econtext = wstate->ps.ps_ExprContext;
	MemoryContext oldctx;
	bool		isnull,
				result;

	Assert(IsA(eq_exprstate->expr, OpExpr));

	/* Make sure Var in eq_expr points to the current slot */
	econtext->ecxt_outertuple = wstate->curslot;

	oldctx = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);
	result = DatumGetBool(ExecEvalExpr(eq_exprstate, econtext, &isnull, NULL));
	MemoryContextSwitchTo(oldctx);

	return isnull ? false : result;
}

/*
 * setEmptyFrame -- set empty_frame flag if the framing clause
 * contains no tuples.
 */
static void
setEmptyFrame(WindowStatePerLevel level_state,
			  WindowState * wstate)
{
	level_state->empty_frame = false;

	Assert((level_state->frameOptions & FRAMEOPTION_END_UNBOUNDED_PRECEDING) == 0);
	Assert((level_state->frameOptions & FRAMEOPTION_START_UNBOUNDED_FOLLOWING) == 0);
	Assert(!(EDGE_TRAIL_IS_BOUND_FOLLOWING(level_state) &&
			 EDGE_LEAD_IS_BOUND_PRECEDING(level_state)));

	if (!level_state->has_delay_trail &&
		!level_state->has_delay_lead &&
		((EDGE_TRAIL_IS_BOUND_PRECEDING(level_state) &&
		  EDGE_LEAD_IS_BOUND_PRECEDING(level_state)) ||
		 (EDGE_TRAIL_IS_BOUND_FOLLOWING(level_state) &&
		  EDGE_LEAD_IS_BOUND_FOLLOWING(level_state))))
	{
		bool		is_pre =
		(EDGE_TRAIL_IS_BOUND_PRECEDING(level_state) &&
		 EDGE_LEAD_IS_BOUND_PRECEDING(level_state));

		if (EDGE_IS_ROWS(level_state))
		{
			if (level_state->lead_rows < level_state->trail_rows)
				level_state->empty_frame = true;
		}
		else
		{
			Oid			ineq_ordfuncid;
			Oid			eq_ordfuncid;
			Datum		ineq_datum;
			Datum		eq_datum;
			FmgrInfo	ineq_fcinfo;
			FmgrInfo	eq_fcinfo;
			Operator	ineq_optup;
			Operator	eq_optup;

			ineq_optup = ordering_oper(exprType((Node *) level_state->trail_expr->expr),
									   false);
			ineq_ordfuncid = oprfuncid(ineq_optup);
			ReleaseSysCache(ineq_optup);
			fmgr_info(ineq_ordfuncid, &ineq_fcinfo);

			eq_optup = equality_oper(exprType((Node *) level_state->trail_expr->expr),
									 false);
			eq_ordfuncid = oprfuncid(eq_optup);
			ReleaseSysCache(eq_optup);
			fmgr_info(eq_ordfuncid, &eq_fcinfo);

			/* is trail less than or equal to lead */
			ineq_datum = FunctionCall2(&ineq_fcinfo, level_state->trail_range,
									   level_state->lead_range);
			eq_datum = FunctionCall2(&eq_fcinfo, level_state->trail_range,
									 level_state->lead_range);
			if (is_pre)
			{
				if (DatumGetBool(ineq_datum) && !DatumGetBool(eq_datum))
					level_state->empty_frame = true;
			}
			else
			{
				if (!DatumGetBool(ineq_datum) && !DatumGetBool(eq_datum))
					level_state->empty_frame = true;
			}
		}
	}
}

/*
 * Initialise frame specific state.
 */

static void
init_frames(WindowState *wstate)
{
	ListCell   *lc;
	int			col_no;

	if (!wstate->trivial_frame)
	{
		WindowStatePerLevel level_state = wstate->level_state;
		int			ncols;

		foreach(lc, level_state->level_funcs)
		{
			WindowStatePerFunction funcstate =
				(WindowStatePerFunction) lfirst(lc);

			if (!funcstate->plain_agg && !funcstate->allowframe &&
				!IS_LEAD_LAG(funcstate->wfuncstate->winkind) &&
				!funcstate->winpeercount)
			{
				funcstate->trivial_frame = true;
			}
			/*
			 * We say the function is trivial to invoke if its frame is
			 * defined as ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
			 * with no exclusion clause OR it is a window function without a
			 * frame.
			 */
			if (funcstate->plain_agg &&
				(level_state->frameOptions & FRAMEOPTION_START_UNBOUNDED_PRECEDING) != 0 &&
				(level_state->frameOptions & FRAMEOPTION_END_CURRENT_ROW) != 0)
			{
				if (EDGE_IS_ROWS(level_state))
					funcstate->trivial_frame = true;
				else
					funcstate->cumul_frame = true;
			}
		}

		/* what sort order did the user specify for each key? */
		ncols = level_state->numSortCols;
		level_state->col_sort_asc = palloc(sizeof(bool) * ncols);
		for (col_no = 0; col_no < ncols; col_no++)
		{
			Oid			sortop = level_state->sortOperators[col_no];
			HeapTuple	opertup;
			Form_pg_operator opform;

			opertup = SearchSysCache1(OPEROID, ObjectIdGetDatum(sortop));
			Insist(opertup);
			opform = (Form_pg_operator) GETSTRUCT(opertup);

			if (strcmp(NameStr(opform->oprname), "<") == 0)
				level_state->col_sort_asc[col_no] = true;
			else
				level_state->col_sort_asc[col_no] = false;

			ReleaseSysCache(opertup);
		}

		/*
		 * If a bound expression is a constant, we can evaluate it just
		 * once, and reuse the value thereafter. Otherwise, set the
		 * has_delay_[trail|lead] flag, so that it's re-evaluated for
		 * every row.
		 *
		 * We could try harder, and also evaluate e.g. stable expressions
		 * immediately, but this will do for now.
		 */
		if (EDGE_TRAIL_IS_BOUND(level_state) &&
			!IsA(level_state->startOffset, Const))
			level_state->has_delay_trail = true;

		if (EDGE_LEAD_IS_BOUND(level_state) &&
			!IsA(level_state->endOffset, Const))
			level_state->has_delay_lead = true;

		level_state->has_delay_bound =
			level_state->has_delay_lead || level_state->has_delay_trail;

		/* now, initialize the actual frame */
		if (EDGE_IS_ROWS(level_state))
		{
			ExprContext *econtext = wstate->ps.ps_ExprContext;

			if (EDGE_TRAIL_IS_BOUND(level_state) &&
				level_state->startOffset != NULL)
			{
				int64		rows_param = 0;
				bool		isnull = true;
				Expr	   *expr = (Expr *) level_state->startOffset;

				Assert(exprType((Node *) expr) == INT8OID);

				level_state->trail_expr =
					ExecInitExpr((Expr *)expr,
								 (PlanState *) wstate);

				if (!level_state->has_delay_trail)
				{
					Datum		d;

					d = ExecEvalExpr(level_state->trail_expr,
									 econtext,
									 &isnull,
									 NULL);
					if (isnull)
						ereport(ERROR,
								(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
								 errmsg("frame starting offset must not be null")));
					rows_param = DatumGetInt64(d);
					if (rows_param < 0)
						ereport(ERROR,
								(errcode(ERROR_INVALID_WINDOW_FRAME_PARAMETER),
								 errmsg("ROWS parameter cannot be negative")));
				}

				if (EDGE_TRAIL_IS_BOUND_PRECEDING(level_state))
					rows_param = -rows_param;

				level_state->trail_rows = rows_param;
			}
			else
				level_state->trail_rows = 0L;

			if (EDGE_LEAD_IS_BOUND(level_state) &&
				level_state->endOffset != NULL)
			{
				int64		rows_param = 0;
				bool		isnull = true;
				Expr	   *expr = (Expr *) level_state->endOffset;

				Assert(exprType((Node *) expr) == INT8OID);

				level_state->lead_expr =
					ExecInitExpr((Expr *)expr,
								 (PlanState *) wstate);

				if (!level_state->has_delay_lead)
				{
					Datum		d;

					d = ExecEvalExpr(level_state->lead_expr,
									 econtext,
									 &isnull,
									 NULL);
					if (isnull)
						ereport(ERROR,
								(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
								 errmsg("frame ending offset must not be null")));
					rows_param = DatumGetInt64(d);
					if (rows_param < 0)
						ereport(ERROR,
								(errcode(ERROR_INVALID_WINDOW_FRAME_PARAMETER),
								 errmsg("ROWS parameter cannot be negative")));
				}

				if (EDGE_LEAD_IS_BOUND_PRECEDING(level_state))
					rows_param = -rows_param;

				level_state->lead_rows = rows_param;
			}
			else
				level_state->lead_rows = 0L;
		}
		else
		{
			TupleDesc	desc = ExecGetResultType(wstate->ps.lefttree);

			/* we only need the subtraction function for bound frames */
			if (EDGE_TRAIL_IS_BOUND(level_state))
			{
				init_bound_frame_edge_expr(level_state, desc,
										   level_state->sortColIdx[0],
										   EDGE_TRAIL, wstate);
			}

			if (EDGE_LEAD_IS_BOUND(level_state))
			{
				init_bound_frame_edge_expr(level_state, desc,
										   level_state->sortColIdx[0],
										   EDGE_LEAD, wstate);
			}
		}

		/* look for empty frames */
		setEmptyFrame(level_state, wstate);
	}

	/*
	 * Set trivial_frames_only in each level state if its functions all have
	 * trivial frames.
	 *
	 * Also initialize the transition values for functions in this level.
	 */
	if (!wstate->trivial_frame)
	{
		bool		trivial_frames_only = true;
		WindowStatePerLevel level_state = wstate->level_state;

		foreach(lc, level_state->level_funcs)
		{
			WindowStatePerFunction funcstate = (WindowStatePerFunction)
				lfirst(lc);

			if (!funcstate->trivial_frame)
			{
				trivial_frames_only = false;
				break;
			}
		}

		level_state->trivial_frames_only = trivial_frames_only;

		resetTransValues(level_state, wstate);
	}

	/*
	 * Set has_only_trans_funcs in each level state if all functions in this
	 * level are aggregate functions that have no preliminary functions or
	 * inverse preliminary functions.
	 */
	if (!wstate->trivial_frame)
	{
		WindowStatePerLevel level_state = wstate->level_state;
		bool		has_only_trans_funcs = false;
		int			funcno = 0;

		foreach(lc, level_state->level_funcs)
		{
			WindowStatePerFunction funcstate =
			(WindowStatePerFunction) lfirst(lc);

			Assert(NULL != level_state->level_funcs);

			if (HAS_ONLY_TRANS_FUNC(funcstate))
			{
				Assert(funcno == 0 || has_only_trans_funcs);
				has_only_trans_funcs = true;

				/*
				 * XXX disable the general case for now. Some work needs to be
				 * done when adjusting edges after appending values into the
				 * frame buffer.
				 */
				ereport(ERROR,
						(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
						 errmsg("aggregate functions with no prelimfn or "
					"invprelimfn are not yet supported as window functions")));
			}

			funcno++;
		}
		level_state->has_only_trans_funcs = has_only_trans_funcs;
	}

	/*
	 * Allocate one FrameBufferEntry buffer for each level state, so that we
	 * don't need to do pallocs/pfrees every time we read an entry from the
	 * frame buffer.
	 */
	if (!wstate->trivial_frame)
	{
		WindowStatePerLevel level_state = wstate->level_state;

		level_state->curr_entry_buf = createFrameBufferEntry(level_state);
		level_state->trail_entry_buf = createFrameBufferEntry(level_state);
		level_state->lead_entry_buf = createFrameBufferEntry(level_state);
	}
}

static FmgrInfo *
get_ltfuncs(int numCols, Oid *ltOperators)
{
	FmgrInfo   *ltfunctions = (FmgrInfo *)palloc(numCols * sizeof(FmgrInfo));
	int			i;

	for (i = 0; i < numCols; i++)
	{
		Oid			lt_function;

		lt_function = get_opcode(ltOperators[i]);
		fmgr_info(lt_function, &ltfunctions[i]);
	}

	return ltfunctions;
}

/*
 * The buffer cursor abstracts a scan on the frame.  The tricky part is that
 * the underlying tuplestore may or may not have rows while in some circumstances
 * we should look at the input buffer as the last row, which is called "last_agg."
 * The usage of these APIs is straightforward; BeginScan, Next, EndScan.  Don't
 * open multiple cursors at one time on the same window level, as the cursor
 * has a transient tuplestore reader.
 * The forward parameter indicates if this cursor starts at the beginning and scans
 * forward, or if it starts at the end of frame and scans backward.
 */
static WindowBufferCursor
windowBufferBeginScan(WindowState * wstate, WindowStatePerLevel level_state,
					  bool forward)
{
	WindowBufferCursor cursor;
	bool		include_last_agg = false;

	cursor = (WindowBufferCursor) palloc0(sizeof(WindowBufferCursorData));

	/*
	 * Because it is a short-term use, we don't allocate a new accessor but
	 * use the existing one to scan the tuplestore.  The other side of reader
	 * should remain so that detection of frame end can work correctly (i.e.
	 * if both reader intersects, it means the end of frame.)
	 *
	 * Remember the original position of accessor so later we can restore it.
	 */
	if (forward)
	{
		(void)ntuplestore_acc_tell(level_state->trail_reader,
								   &cursor->orig_pos);
		cursor->reader = level_state->trail_reader;
	}
	else
	{
		(void)ntuplestore_acc_tell(level_state->lead_reader,
								   &cursor->orig_pos);
		cursor->reader = level_state->lead_reader;
	}

	/*
	 * This is a little complicated condition to tell if the last_agg row
	 * should be looked at.  After setting use_last_agg, we'll return last_agg
	 * just after finishing to return all the rows in the tuplestore.
	 */
	if (EDGE_TRAIL_EQ_CURRENT_ROW(level_state, wstate))
		include_last_agg = true;
	if (!EDGE_LEAD_IS_BOUND(level_state) ||
		EDGE_LEAD_IS_BOUND_FOLLOWING(level_state) ||
		EDGE_LEAD_EQ_CURRENT_ROW(level_state, wstate))
	{
		include_last_agg = true;
	}
	cursor->use_last_agg = (!ntuplestore_acc_tell(level_state->lead_reader, NULL) &&
							level_state->agg_filled && include_last_agg);

	/*
	 * Also we check if this frame has tuples.	If this is false, entire frame
	 * is empty and nothing should be returned.
	 */
	cursor->has_tuples = hasTuplesInFrame(level_state, wstate);

	if (forward)
	{
		/*
		 * Since the trail_reader points to the value before the trailing
		 * edge, we advance the trail_reader by one.
		 */
		if (ntuplestore_acc_tell(cursor->reader, NULL))
			ntuplestore_acc_advance(cursor->reader, 1);
		else
		{
			/*
			 * In forward, if the leader is at invalid position and the
			 * leading edge is not equal to the current as PRECEDING bound, we
			 * return nothing.
			 */
			if (!ntuplestore_acc_tell(level_state->lead_reader, NULL) &&
				(EDGE_LEAD_IS_BOUND_PRECEDING(level_state) &&
				 ((EDGE_IS_ROWS(level_state) && level_state->lead_rows != 0) ||
				  (!EDGE_IS_ROWS(level_state) && !exec_eq_exprstate(wstate,
										 level_state->lead_range_eq_expr)))))
				cursor->has_tuples = false;
			else
				ntuplestore_acc_seek_first(cursor->reader);
		}
	}
	else
	{
		/*
		 * In backward, make sure the reader is on the last.
		 */
		if (!ntuplestore_acc_tell(cursor->reader, NULL))
			ntuplestore_acc_seek_last(cursor->reader);
	}

	cursor->first = true;
	cursor->level_state = level_state;
	cursor->wstate = wstate;
	cursor->forward = forward;

	return cursor;
}

/*
 * A helper of windowBufferNext.  Scans and advances in the tuplestore.
 * If it has reached to the end, sets reader_eof to true and returns NULL.
 * Otherwise returns a valid List containing serialized WindowValues.
 */
static List *
windowBufferNextTupleStore(WindowBufferCursor cursor)
{
	WindowStatePerLevel level_state = cursor->level_state;
	FrameBufferEntry *entry = level_state->curr_entry_buf;

	if (!cursor->reader_eof)
	{
		if (!cursor->first)
			ntuplestore_acc_advance(cursor->reader, cursor->forward ? 1 : -1);
		cursor->first = false;
		if (!ntuplestore_acc_tell(cursor->reader, NULL))
			cursor->reader_eof = true;
		else
		{
			/*
			 * If the reader intersects the other side, it means the end of
			 * frame.
			 */
			if (cursor->forward)
			{
				if (ntuplestore_acc_tell(level_state->lead_reader, NULL) &&
					ntuplestore_acc_is_before(level_state->lead_reader,
											  cursor->reader))
					cursor->reader_eof = true;
			}
			else
			{
				if (ntuplestore_acc_tell(level_state->trail_reader, NULL) &&
					ntuplestore_acc_is_before(cursor->reader,
											  level_state->trail_reader))
					cursor->reader_eof = true;
			}
		}
	}

	if (!cursor->reader_eof)
	{
		bool		found;

		found = getCurrentValue(cursor->reader, level_state, entry);

		/*
		 * As we are sure the reader is at a valid position, the entry must be
		 * found.
		 */
		Assert(found);

		return entry->func_values;
	}

	return NULL;
}

/*
 * A helper of windowBufferNext.  Scans "last_agg" if we should, and converts the
 * input buffer to a List of WindowValues.	Since last_agg is the last row of
 * the frame if any, we set the use_last_agg to false after scanning and will never
 * get the values again.
 */
static List *
windowBufferNextLastAgg(WindowBufferCursor cursor)
{
	if (cursor->use_last_agg)
	{
		WindowStatePerLevel level_state = cursor->level_state;
		WindowState *wstate = cursor->wstate;
		FrameBufferEntry *entry = level_state->curr_entry_buf;
		bool		found;
		ExprContext *econtext = wstate->ps.ps_ExprContext;
		TupleTableSlot *slot = econtext->ecxt_outertuple;

		found = ntuplestore_acc_current_tupleslot(wstate->input_buffer->writer,
												  wstate->spare);
		Assert(found);

		/*
		 * Serialize and deserialize the input buffer.	This seems a little
		 * redundant, but the format of input buffer and WindowValue is
		 * different, and as we want the same format values from the buffer
		 * cursor, we do this here. It is actually not so bad, as it's done in
		 * memory.
		 */
		econtext->ecxt_outertuple = wstate->spare;
		resetStringInfo(&level_state->serial_buf);
		serializeEntry(level_state, econtext,
					   &level_state->serial_buf);
		entry = deserializeEntry(level_state, entry,
								 level_state->serial_buf.data,
								 level_state->serial_buf.len);
		/* Get back the slot. */
		econtext->ecxt_outertuple = slot;
		/* We never use this again as it's the logical last row. */
		cursor->use_last_agg = false;

		return entry->func_values;
	}

	return NULL;
}

/*
 * Scans the frame buffer and returns the current value.  It advances the cursor
 * 1 forward or backward.  If the scan is finished, it returns NULL.
 */
static List *
windowBufferNext(WindowBufferCursor cursor)
{
	List	   *func_values;

	if (!cursor->has_tuples)
		return NULL;

	if (cursor->forward)
	{
		/*
		 * In forward, start from tuplestore and use last_agg after that.
		 */
		func_values = windowBufferNextTupleStore(cursor);
		if (func_values)
			return func_values;
		return windowBufferNextLastAgg(cursor);
	}
	else
	{
		/*
		 * In backward, start from last_agg and use tuplestore after that.
		 */
		func_values = windowBufferNextLastAgg(cursor);
		if (func_values)
			return func_values;
		return windowBufferNextTupleStore(cursor);
	}
}

/*
 * Ends the cursor.  The cursor will be freed.	The reader will be ok to use again.
 */
static void
windowBufferEndScan(WindowBufferCursor cursor)
{
	if (!ntuplestore_acc_seek(cursor->reader, &cursor->orig_pos))
		ntuplestore_acc_set_invalid(cursor->reader);
	pfree(cursor);
}


/* -----------------
 * ExecInitWindow
 *
 *	Creates the run-time information for the window node produced by the
 *	planner and initializes its outer subtree
 * -----------------
 */
WindowState *
ExecInitWindow(WindowAgg * node, EState *estate, int eflags)
{
	WindowState *wstate;
	Plan	   *outerPlan;
	ExprContext *econtext;
	int			numrefs;
	TupleDesc	desc;

	/* Check for unsupported flags. */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	/* Create state structure. */
	wstate = makeWindowState(node, estate);

	/* Create expression context. */
	ExecAssignExprContext(estate, &wstate->ps);

	/* XXX: temporarily high for debugging purposes */
#define WINDOW_NSLOTS 4

	/* Initialize tuple table. */
	ExecInitResultTupleSlot(estate, &wstate->ps);
	wstate->curslot = ExecInitExtraTupleSlot(estate);
	wstate->priorslot = ExecInitExtraTupleSlot(estate);
	wstate->spare = ExecInitExtraTupleSlot(estate);

	/*
	 * Initialize child expressions.
	 *
	 * ExecInitExpr adds WindowFuncExprState nodes it encounters to the wfxstates
	 * list.
	 */
	wstate->ps.targetlist = (List *)
		ExecInitExpr((Expr *)node->plan.targetlist,
					 (PlanState *) wstate);
	numrefs = list_length(wstate->wfxstates);

	/* Initialize child nodes. */
	outerPlan = outerPlan(node);
	outerPlanState(wstate) = ExecInitNode(outerPlan, estate, eflags);
	Assert(innerPlan(node) == NULL);

	/* Initialize result tuple type and projection info. */
	ExecAssignResultTypeFromTL(&wstate->ps);
	ExecAssignProjectionInfo(&wstate->ps, NULL);

	desc = ExecGetResultType(wstate->ps.lefttree);

	/* Precompute fmgr lookup data for partition key equality function. */
	if (node->partNumCols > 0)
	{
		wstate->eqfunctions =
			execTuplesMatchPrepare(node->partNumCols,
								   node->partOperators);
	}
	else
		wstate->eqfunctions = NULL;

	if (!wstate->trivial_frame)
		initWindowStatePerLevel(wstate, node);

	/*
	 * Allocate result storage and working storage per window ref. (Later we
	 * may shrink this if we notice duplicate function calls, but allocate for
	 * worst case.)  Note that we borrow aggregate result storage, since there
	 * are no non-window aggregates that might use it.
	 */
	econtext = wstate->ps.ps_ExprContext;
	econtext->ecxt_aggvalues = (Datum *)palloc0(sizeof(Datum) * numrefs);
	econtext->ecxt_aggnulls = (bool *)palloc0(sizeof(bool) * numrefs);

	wstate->need_peercount = false;

	initWindowFuncState(wstate, node);

	wstate->mem_manager.alloc = cxt_alloc;
	wstate->mem_manager.free = cxt_free;
	wstate->mem_manager.manager = wstate->transcontext;

	/* Frame initialisation can take place now */
	init_frames(wstate);

	return wstate;
}


int
ExecCountSlotsWindow(WindowAgg *node)
{
	return ExecCountSlotsNode(outerPlan(node)) +
		ExecCountSlotsNode(innerPlan(node)) +
		WINDOW_NSLOTS;
}

void
ExecEndWindow(WindowState * node)
{
	if (!node->trivial_frame)
	{
		WindowStatePerLevel level_state = node->level_state;

		freeFrameBufferEntry(level_state->curr_entry_buf);
		level_state->curr_entry_buf = NULL;

		freeFrameBufferEntry(level_state->trail_entry_buf);
		level_state->trail_entry_buf = NULL;

		freeFrameBufferEntry(level_state->lead_entry_buf);
		level_state->lead_entry_buf = NULL;

		if (level_state->serial_buf.data != NULL)
		{
			pfree(level_state->serial_buf.data);
			level_state->serial_buf.data = NULL;
		}
	}

	ExecEagerFreeWindow(node);

	/* Free the exprcontext */
	ExecFreeExprContext(&node->ps);

	/* clean out the tuple table */
	ExecClearTuple(node->curslot);
	ExecClearTuple(node->spare);
	ExecClearTuple(node->priorslot);
	ExecClearTuple(node->ps.ps_ResultTupleSlot);

	if (node->transcontext != NULL)
		MemoryContextDelete(node->transcontext);

	if (node->cmpcontext != NULL)
		MemoryContextDelete(node->cmpcontext);

	if (!node->trivial_frame)
		pfree(node->level_state);
	if (node->func_state != NULL)
		pfree(node->func_state);

	/* shut down subplans */
	ExecEndNode(outerPlanState(node));

	EndPlanStateGpmonPkt(&node->ps);
}

/**
 * ExecReScanWindow
 *
 * Higher-up node is telling window node to perform re-scanning.
 * Note that exprCtxt may be NULL.
 */
void
ExecReScanWindow(WindowState * node, ExprContext *exprCtxt)
{
	Assert(node);

	resetFrameBuffers(node);

	ExecEagerFreeWindow(node);

	node->is_input_done = false;

	Assert(outerPlanState(node));

	ExecReScan(outerPlanState(node), exprCtxt);
}

/*
 * initialize_peragg
 *
 * Almost same as in nodeAgg.c, except we don't support DISTINCT currently.
 */
static void
initialize_peragg(WindowState *winstate, WindowFunc *wfunc,
				  WindowStatePerFunction funcstate)
{
	Oid			inputTypes[FUNC_MAX_ARGS];
	int			numArguments;
	HeapTuple	aggTuple;
	Form_pg_aggregate aggform;
	Oid			aggtranstype;
	AclResult	aclresult;
	Oid			transfn_oid,
				finalfn_oid,
				invtransfn_oid,
				invprelimfn_oid,
				prelimfn_oid;
	Expr	   *transfnexpr,
			   *finalfnexpr,
			   *invtransfnexpr,
			   *invprelimfnexpr,
			   *prelimfnexpr;
	Datum		textInitVal;
	int			i;
	ListCell   *lc;

	numArguments = list_length(wfunc->args);

	i = 0;
	foreach(lc, wfunc->args)
	{
		inputTypes[i++] = exprType((Node *) lfirst(lc));
	}

	aggTuple = SearchSysCache(AGGFNOID,
							  ObjectIdGetDatum(wfunc->winfnoid),
							  0, 0, 0);
	if (!HeapTupleIsValid(aggTuple))
		elog(ERROR, "cache lookup failed for aggregate %u",
			 wfunc->winfnoid);
	aggform = (Form_pg_aggregate) GETSTRUCT(aggTuple);

	/*
	 * ExecInitWindowAgg already checked permission to call aggregate function
	 * ... but we still need to check the component functions
	 */

	funcstate->transfn_oid = transfn_oid = aggform->aggtransfn;
	funcstate->finalfn_oid = finalfn_oid = aggform->aggfinalfn;
	funcstate->invtransfn_oid = invtransfn_oid = aggform->agginvtransfn;
	funcstate->invprelimfn_oid = invprelimfn_oid = aggform->agginvprelimfn;
	funcstate->prelimfn_oid = prelimfn_oid = aggform->aggprelimfn;

	/* Check that aggregate owner has permission to call component fns */
	{
		Oid			to_check[] = {transfn_oid,
								  finalfn_oid,
								  invtransfn_oid,
								  invprelimfn_oid,
								  prelimfn_oid};
		HeapTuple	procTuple;
		Oid			aggOwner;

		procTuple = SearchSysCache1(PROCOID,
									ObjectIdGetDatum(wfunc->winfnoid));
		if (!HeapTupleIsValid(procTuple))
			elog(ERROR, "cache lookup failed for function %u",
				 wfunc->winfnoid);
		aggOwner = ((Form_pg_proc) GETSTRUCT(procTuple))->proowner;
		ReleaseSysCache(procTuple);

		for (i = 0; i < lengthof(to_check); i++)
		{
			if (!OidIsValid(to_check[i]))
				continue;
			aclresult = pg_proc_aclcheck(to_check[i], aggOwner,
										 ACL_EXECUTE);
			if (aclresult != ACLCHECK_OK)
				aclcheck_error(aclresult, ACL_KIND_PROC,
							   get_func_name(to_check[i]));
		}
	}

	aggtranstype = resolve_polymorphic_transtype(aggform->aggtranstype,
												 wfunc->winfnoid,
												 inputTypes);

	/* build expression trees using actual argument & result types */
	build_aggregate_fnexprs(inputTypes,
							numArguments,
							aggtranstype,
							wfunc->wintype,
							transfn_oid,
							finalfn_oid,
							prelimfn_oid,
							invtransfn_oid,
							invprelimfn_oid,
							&transfnexpr,
							&finalfnexpr,
							&prelimfnexpr,
							&invtransfnexpr,
							&invprelimfnexpr);

	fmgr_info(funcstate->transfn_oid, &funcstate->transfn);
	funcstate->transfn.fn_expr = (Node *) transfnexpr;

	if (OidIsValid(finalfn_oid))
	{
		fmgr_info(finalfn_oid, &funcstate->finalfn);
		funcstate->finalfn.fn_expr = (Node *) finalfnexpr;
	}
	if (OidIsValid(invtransfn_oid))
	{
		fmgr_info(invtransfn_oid, &funcstate->invtransfn);
		funcstate->invtransfn.fn_expr = (Node *) invtransfnexpr;
	}
	if (OidIsValid(prelimfn_oid))
	{
		fmgr_info(prelimfn_oid, &funcstate->prelimfn);
		funcstate->prelimfn.fn_expr = (Node *) prelimfnexpr;
	}
	if (OidIsValid(invprelimfn_oid))
	{
		fmgr_info(invprelimfn_oid, &funcstate->invprelimfn);
		funcstate->invprelimfn.fn_expr = (Node *) invprelimfnexpr;
	}

	get_typlenbyval(wfunc->wintype,
					&funcstate->resulttypeLen,
					&funcstate->resulttypeByVal);
	get_typlenbyval(aggtranstype,
					&funcstate->aggTranstypeLen,
					&funcstate->aggTranstypeByVal);

	/*
	 * initval is potentially null, so don't try to access it as a struct
	 * field. Must do it the hard way with SysCacheGetAttr.
	 */
	textInitVal = SysCacheGetAttr(AGGFNOID, aggTuple,
								  Anum_pg_aggregate_agginitval,
								  &funcstate->aggInitValueIsNull);

	if (funcstate->aggInitValueIsNull)
		funcstate->aggInitValue = (Datum) 0;
	else
		funcstate->aggInitValue = GetAggInitVal(textInitVal,
												aggtranstype);

	/*
	 * If the transfn is strict and the initval is NULL, make sure input type
	 * and transtype are the same (or at least binary-compatible), so that
	 * it's OK to use the first input value as the initial transValue.  This
	 * should have been checked at agg definition time, but just in case...
	 */
	if (funcstate->transfn.fn_strict && funcstate->aggInitValueIsNull)
	{
		if (numArguments < 1 ||
			!IsBinaryCoercible(inputTypes[0], aggtranstype))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_FUNCTION_DEFINITION),
					 errmsg("aggregate %u needs to have compatible input type and transition type",
							wfunc->winfnoid)));
	}

	ReleaseSysCache(aggTuple);
}


/*
 * window_dummy - dummy execution routine for window functions
 *
 * This function is listed as the implementation (prosrc field) of pg_proc
 * entries for window functions.  Its only purpose is to throw an error
 * if someone mistakenly executes such a function in the normal way.
 */
Datum
window_dummy(PG_FUNCTION_ARGS)
{
	ereport(ERROR,
			(errcode(ERRCODE_SYNTAX_ERROR),
			 errmsg("function %s may only be called as a window function",
					format_procedure(fcinfo->flinfo->fn_oid)),
	  errhint("To call a function as a window function use an OVER clause."))
		);

	return (Datum)0;			/* keep compiler quiet */
}

/*
 * Implements the gp_execution_segment() function to return the contentid
 * of the current executing segment.
 */
Datum
mpp_execution_segment(PG_FUNCTION_ARGS)
{
	PG_RETURN_INT32(Gp_segment);
}

/*
 * Implements the gp_execution_dbid() function to return the dbid of the
 * current executing segment.
 */
Datum
gp_execution_dbid(PG_FUNCTION_ARGS)
{
	PG_RETURN_INT32(GpIdentity.dbid);
}

/*
 * ROW_NUMBER() OVER (...) --> BIGINT
 *
 * Implement ROW_NUMBER for the given window state:
 *
 * row_number_immed(internal) --> bigint
 */
Datum
row_number_immed(PG_FUNCTION_ARGS)
{
	WindowFuncExprState *ref_state = (WindowFuncExprState *) PG_GETARG_POINTER(0);

	int64		result = 1 + ref_state->windowstate->row_index;

	PG_RETURN_INT64(result);
}


/*
 *	RANK() OVER (... ORDER BY s) --> BIGINT
 *
 * Implement RANK for the given WindowFuncExprState.
 *
 * rank_immed(internal) --> bigint
 */
Datum
rank_immed(PG_FUNCTION_ARGS)
{
	int64		result;
	WindowFuncExprState *ref_state = (WindowFuncExprState *) PG_GETARG_POINTER(0);
	WindowState *window_state = ref_state->windowstate;
	WindowStatePerLevel level_state = window_state->level_state;

	/* Don't advance prior_rank here, let the framework do this. */

	result = level_state->rank;

	PG_RETURN_INT64(result);
}


/*
 *	DENSE_RANK() OVER (... ORDER BY s) --> BIGINT
 *
 * Implement DENSE_RANK for the given WindowFuncExprState.
 *
 * dense_rank_immed(internal) --> bigint
 */
Datum
dense_rank_immed(PG_FUNCTION_ARGS)
{
	int64		result;
	WindowFuncExprState *ref_state = (WindowFuncExprState *) PG_GETARG_POINTER(0);
	WindowState *window_state = ref_state->windowstate;
	WindowStatePerLevel level_state = window_state->level_state;

	/* Don't advance prior_dense_rank here, let the framework do this. */

	result = level_state->dense_rank;

	PG_RETURN_INT64(result);
}


/*
 * PERCENT_RANK() OVER (... ORDER BY s) -- FLOAT8
 *
 * Implement rank for the given WindowFuncExprState.
 *
 * rank_immed(internal) --> bigint
 * percent_rank_final(bigint,bigint) --> float8
 */
Datum
percent_rank_final(PG_FUNCTION_ARGS)
{
	int64		arg1 = PG_GETARG_INT64(0);		/* rank in partition */
	int64		arg2 = PG_GETARG_INT64(1);		/* partition row count */
	double		result;

	if (arg1 < 1 || arg2 < 1)
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("arguments invalid or inconsistent"),
			 errhint("inappropriate call to window function implementation")));
		result = 0.0;			/* quieten GCC */
	}
	else if (arg1 == 1 && arg2 == 1)
	{
		result = 0.0;
	}
	else
	{
		/* Do division in double, then check for overflow */
		result = (double)(arg1 - 1) / (double)(arg2 - 1);
		if (result > DBL_MAX)
		{
			ereport(ERROR,
					(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
					 errmsg("value out of range: overflow")));
		}
	}

	PG_RETURN_FLOAT8(result);
}


/*
 *	CUME_DIST() OVER (... ORDER BY s) --> FLOAT8
 *
 * Implement CUME_DIST for the given WindowFuncExprState.
 *
 * cume_dist_prelim(internal) --> bigint
 * cume_dist_final(bigint,bigint) --> float8
 */
Datum
cume_dist_prelim(PG_FUNCTION_ARGS)
{
	int64		result;
	WindowFuncExprState *ref_state = (WindowFuncExprState *) PG_GETARG_POINTER(0);
	WindowState *window_state = ref_state->windowstate;
	WindowStatePerLevel level_state = window_state->level_state;

	result = level_state->prior_non_peer_count + (level_state->peer_count + 1);

	PG_RETURN_INT64(result);
}

Datum
cume_dist_final(PG_FUNCTION_ARGS)
{
	int64		arg1 = PG_GETARG_INT64(0);		/* prior_non_peer_count +
												 * peer_count */
	int64		arg2 = PG_GETARG_INT64(1);		/* partition row count */
	double		result;

	if (arg1 < 1 || arg2 < 1)
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATA_EXCEPTION),
				 errmsg("arguments invalid or inconsistent"),
		   errhint("inappropriate call to window function implementation")));
		result = 0.0;			/* quieten GCC */
	}
	else if (arg1 == 1 && arg2 == 1)
	{
		result = 1.0;			/* or is it 0.0? */
	}
	else
	{
		/* Do division in double, then check for overflow */
		result = (double)arg1 / (double)arg2;
		if (result > DBL_MAX)
		{
			ereport(ERROR,
					(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
					 errmsg("value out of range: overflow")));
		}
	}

	PG_RETURN_FLOAT8((float8)result);
}



/*
 *	NTILE(BIGINT) OVER (... ORDER BY s) --> BIGINT
 *
 * Implement NTILE for the given WindowFuncExprState.
 *
 * ntile_prelim_int(internal,int) --> bigint[]
 * ntile_prelim_bigint(internal,bigint) --> bigint[]
 * ntile_prelim_numeric(internal,numeric) --> bigint[]
 * ntile_final(bigint[],bigint) --> bigint
 */

/* Helper. */
static ArrayType *
do_ntile_prelim(WindowFuncExprState * ref_state, int64 num_tiles)
{
	/*
	 * Pack row_index from state and the argument num_tiles into a two-element
	 * array of type bigint.
	 */

	Datum		work  [2];

	WindowState *window_state = ref_state->windowstate;

	if (num_tiles <= 0)
		ereport(ERROR,
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				 errmsg("argument value out of range"),
				 errhint("NTILE expects a positive integer argument.")));

	work[0] = Int64GetDatumFast(num_tiles);		/* ntile argument */
	work[1] = Int64GetDatumFast(window_state->row_index);

	return construct_array(work, 2, INT8OID, 8, true, 'd');
}

Datum
ntile_prelim_int(PG_FUNCTION_ARGS)
{
	WindowFuncExprState *rstate = (WindowFuncExprState *) PG_GETARG_POINTER(0);
	int64		ntile_arg = (int64) PG_GETARG_INT32(1);

	PG_RETURN_ARRAYTYPE_P(do_ntile_prelim(rstate, ntile_arg));
}

Datum
ntile_prelim_bigint(PG_FUNCTION_ARGS)
{
	WindowFuncExprState *rstate = (WindowFuncExprState *) PG_GETARG_POINTER(0);
	int64		ntile_arg = PG_GETARG_INT64(1);

	PG_RETURN_ARRAYTYPE_P(do_ntile_prelim(rstate, ntile_arg));
}

Datum
ntile_prelim_numeric(PG_FUNCTION_ARGS)
{
	int64		num_tiles;
	WindowFuncExprState *rstate = (WindowFuncExprState *) PG_GETARG_POINTER(0);
	Numeric		ntile_arg = PG_GETARG_NUMERIC(1);

	/* Truncate ntile_arg to int8, put in num_tiles. */
	num_tiles = numeric_to_pos_int8_trunc(ntile_arg);
	if (num_tiles < 1)
		num_tiles = 0;			/* Set value out of range to trigger error
								 * report. */

	PG_RETURN_ARRAYTYPE_P(do_ntile_prelim(rstate, num_tiles));
}

/*
 * To implement the semantics of NTILE correctly, tuples in the same bucket
 * must be clustered together according to the input ordering --
 * i.e., 1, 1, 1, 2, 2, 2, 3, 3, 4, 4.
 *
 * It's tempting to just do ((N - 1) mod M) + 1 but this will not meet
 * our requirement. Alternatively, we could just divide the partition
 * row count into N buckets and see if the row index fit into an individual
 * bucket. That would give us clustered results but it wouldn't handle the
 * situation where the number of rows in the partition did not divide evenly
 * into the number of buckets (notice that in the above example, these
 * larger-by-one buckets are the leading buckets).
 *
 * So, instead we identify the threshold (prefix_size) to which buckets will
 * have one more row and we divide the rows between them (row_index/max_size).
 * After the threshold, the next bucket will be number 1 + spares and
 * we divide the values evenly between each bucket.
 *
 * Note that row_index counts from 0.
 */

Datum
ntile_final(PG_FUNCTION_ARGS)
{
	int64		result;
	Datum	   *work;
	int			len;
	int64		row_index,
				num_tiles,
				min_size ,
				max_size ,
				prefix_size,
				spares;

	ArrayType  *pair = PG_GETARG_ARRAYTYPE_P(0);		/* row_index, num_tiles */
	int64		partition_row_count = PG_GETARG_INT64(1);		/* partition row count */

	/* we expect the input to be bigint[2] */
	deconstruct_array(pair, INT8OID, 8, true, 'd', &work, NULL, &len);
	if (len != 2)
		elog(ERROR, "expected 2-element int8 array");

	num_tiles = DatumGetInt64(work[0]);
	row_index = DatumGetInt64(work[1]);
	min_size = partition_row_count / num_tiles;

	spares = partition_row_count % num_tiles;
	max_size = min_size + 1;
	prefix_size = spares * max_size;

	if (row_index < prefix_size)
		result = 1 + row_index / max_size;
	else
		result = 1 + spares +
			(row_index - prefix_size) / min_size;

	PG_RETURN_INT64(result);
}

/*
 * Check that the 'offset' argument to LEAD or LAG function is
 * valid.
 */
static void
check_lead_lag_offset(int64 value, bool isnull, bool is_lead)
{
	if (isnull)
	{
		if (is_lead)
			ereport(ERROR,
					(errcode(ERROR_INVALID_WINDOW_FRAME_PARAMETER),
					 errmsg("LEAD offset cannot be NULL")));
		else
			ereport(ERROR,
					(errcode(ERROR_INVALID_WINDOW_FRAME_PARAMETER),
					 errmsg("LAG offset cannot be NULL")));
	}

	if (value < 0)
	{
		if (is_lead)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("LEAD offset cannot be negative")));
		else
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("LAG offset cannot be negative")));
	}

}

/*
 * The common code for lead and lag.
 */
static Datum
lead_lag_internal(PG_FUNCTION_ARGS, bool is_lead, bool *isnull)
{
	WindowFuncExprState *wfxstate =
		(WindowFuncExprState *) PG_GETARG_POINTER(0);
	WindowState *wstate = wfxstate->windowstate;
	WindowStatePerFunction funcstate = &wstate->func_state[wfxstate->funcno];
	WindowStatePerLevel level_state = wstate->level_state;
	WindowBufferCursor cursor;
	List	   *func_values;

	Insist(PG_NARGS() >= 2);
	Assert(EDGE_IS_ROWS(level_state));

	/*
	 * Check the offset argument if given.	It is actually not used to
	 * retrieve the value, as the frame is tracking the offset.
	 */
	if (PG_NARGS() > 2)
	{
		check_lead_lag_offset(PG_GETARG_INT64(2),
							  PG_ARGISNULL(2),
							  is_lead);
	}

	/*
	 * The direction doesn't matter, as the frame has only one row.
	 */
	cursor = windowBufferBeginScan(wstate, level_state, true);
	func_values = windowBufferNext(cursor);

	/*
	 * If this is out of frame, return the default value argument, or NULL if
	 * not given.
	 */
	if (!func_values)
	{
		if (PG_NARGS() > 3)
		{
			funcstate->win_value_is_null = PG_ARGISNULL(3);
			if (!funcstate->win_value_is_null)
				funcstate->win_value = PG_GETARG_DATUM(3);
			else
				funcstate->win_value = (Datum)0;
		}
		else
		{
			funcstate->win_value = (Datum)0;
			funcstate->win_value_is_null = true;
		}
	}
	else
	{
		WindowValue *winvalue;

		winvalue = (WindowValue *)
			list_nth(func_values, funcstate->serial_index);

		if (winvalue)
		{
			funcstate->win_value = winvalue->value;
			funcstate->win_value_is_null = winvalue->valueIsNull;
		}
		else
		{
			funcstate->win_value = (Datum)0;
			funcstate->win_value_is_null = true;
		}
	}
	windowBufferEndScan(cursor);

	*isnull = funcstate->win_value_is_null;

	return funcstate->win_value;
}

/*
 * Initial implementation of LEAD(col, offset, value_expr)
 */
Datum
lead_generic(PG_FUNCTION_ARGS)
{
	bool		isnull;
	Datum		d;

	d = lead_lag_internal(fcinfo, true, &isnull);

	if (isnull)
		PG_RETURN_NULL();

	PG_RETURN_DATUM(d);
}

Datum
lag_generic(PG_FUNCTION_ARGS)
{
	bool		isnull;
	Datum		d;

	d = lead_lag_internal(fcinfo, false, &isnull);

	if (isnull)
		PG_RETURN_NULL();

	PG_RETURN_DATUM(d);
}

static Datum
last_value_internal(WindowFuncExprState *wfxstate, bool *isnull)
{
	WindowState *wstate = wfxstate->windowstate;
	WindowStatePerFunction funcstate = &wstate->func_state[wfxstate->funcno];
	WindowStatePerLevel level_state = wstate->level_state;
	FrameBufferEntry *lead_entry = level_state->lead_entry_buf;
	bool		has_lead_entry = false;
	WindowValue *lead_value = NULL;
	bool		lead_valid = true;
	bool		has_tuples = true;
	bool		include_last_agg = false;

	has_tuples = hasTuplesInFrame(level_state, wstate);

	if (has_tuples)
	{
		if (EDGE_TRAIL_EQ_CURRENT_ROW(level_state, wstate))
			include_last_agg = true;

		if (!EDGE_LEAD_IS_BOUND(level_state) ||
			EDGE_LEAD_IS_BOUND_FOLLOWING(level_state) ||
			EDGE_LEAD_EQ_CURRENT_ROW(level_state, wstate))
			include_last_agg = true;

		if (!level_state->agg_filled &&
			!ntuplestore_acc_tell(level_state->lead_reader, NULL))
		{
			lead_valid = false;
			ntuplestore_acc_seek_last(level_state->lead_reader);
		}

		if (EDGE_IS_ROWS(level_state))
		{
			if (!EDGE_TRAIL_IS_BOUND_FOLLOWING(level_state) ||
				level_state->num_lead_rows >= level_state->trail_rows)
				has_lead_entry = getCurrentValue(level_state->lead_reader,
												 level_state, lead_entry);
		}
		else
		{
			has_lead_entry = getCurrentValue(level_state->lead_reader,
											 level_state, lead_entry);
		}

		if (has_lead_entry)
			lead_value = (WindowValue *) list_nth(lead_entry->func_values,
												  funcstate->serial_index);
	}

	if (lead_value != NULL)
	{
		funcstate->win_value = lead_value->value;
		funcstate->win_value_is_null = lead_value->valueIsNull;
	}

	else
	{
		if (has_tuples && include_last_agg && level_state->agg_filled)
		{
			funcstate->win_value = funcstate->aggTransValue;
			funcstate->win_value_is_null = funcstate->aggTransValueIsNull;
		}

		else
		{
			funcstate->win_value = 0;
			funcstate->win_value_is_null = true;
		}
	}

	*isnull = funcstate->win_value_is_null;

	if (!lead_valid)
		ntuplestore_acc_set_invalid(level_state->lead_reader);

	return funcstate->win_value;
}

Datum
last_value_generic(PG_FUNCTION_ARGS)
{
	Datum		d;
	bool		isnull = false;

	d = last_value_internal((WindowFuncExprState *) PG_GETARG_POINTER(0),
							&isnull);

	if (isnull)
		PG_RETURN_NULL();

	return d;
}

static Datum
first_value_internal(WindowFuncExprState *wfxstate, bool *isnull)
{
	WindowState *wstate = wfxstate->windowstate;
	WindowStatePerFunction funcstate = &wstate->func_state[wfxstate->funcno];
	WindowStatePerLevel level_state = wstate->level_state;
	FrameBufferEntry *trail_entry = level_state->trail_entry_buf;
	bool		has_trail_entry = false;
	WindowValue *trail_value = NULL;
	NTupleStorePos orig_pos;
	bool		trail_valid;
	bool		has_tuples = true;

	has_tuples = hasTuplesInFrame(level_state, wstate);

	if (!has_tuples)
	{
		funcstate->win_value = 0;
		funcstate->win_value_is_null = true;

		*isnull = true;
		return funcstate->win_value;
	}

	/* Save the position for the trail_reader */
	trail_valid = ntuplestore_acc_tell(level_state->trail_reader, &orig_pos);

	/*
	 * Since the trail_reader points to the value that is right before the
	 * trailing edge, we advance the trail_reader by one.
	 */
	if (trail_valid)
		ntuplestore_acc_advance(level_state->trail_reader, 1);
	else
	{
		if (!EDGE_TRAIL_IS_BOUND_FOLLOWING(level_state) ||
			EDGE_TRAIL_EQ_CURRENT_ROW(level_state, wstate))
			ntuplestore_acc_seek_first(level_state->trail_reader);
	}

	if (EDGE_IS_ROWS(level_state))
	{
		if (!EDGE_TRAIL_IS_BOUND_FOLLOWING(level_state) ||
			level_state->num_lead_rows >= level_state->trail_rows)
			has_trail_entry = getCurrentValue(level_state->trail_reader,
											  level_state, trail_entry);
	}
	else
	{
		has_trail_entry = getCurrentValue(level_state->trail_reader,
										  level_state, trail_entry);
	}

	if (has_trail_entry)
		trail_value = (WindowValue *) list_nth(trail_entry->func_values,
											   funcstate->serial_index);

	if (trail_value != NULL)
	{
		funcstate->win_value = trail_value->value;
		funcstate->win_value_is_null = trail_value->valueIsNull;
	}

	else
	{
		bool		include_last_agg = false;

		if (EDGE_TRAIL_EQ_CURRENT_ROW(level_state, wstate))
			include_last_agg = true;

		if (EDGE_LEAD_EQ_CURRENT_ROW(level_state, wstate))
			include_last_agg = true;

		if ((has_tuples || include_last_agg || trail_valid) &&
			level_state->agg_filled)
		{
			funcstate->win_value = funcstate->aggTransValue;
			funcstate->win_value_is_null = funcstate->aggTransValueIsNull;
		}

		else
		{
			funcstate->win_value = 0;
			funcstate->win_value_is_null = true;
		}
	}

	/* Reset the trail_reader to its original position. */
	if (!ntuplestore_acc_seek(level_state->trail_reader, &orig_pos))
		ntuplestore_acc_set_invalid(level_state->trail_reader);

	*isnull = funcstate->win_value_is_null;

	return funcstate->win_value;
}

Datum
first_value_generic(PG_FUNCTION_ARGS)
{
	Datum		d;
	bool		isnull = false;

	d = first_value_internal((WindowFuncExprState *) PG_GETARG_POINTER(0),
							 &isnull);

	if (isnull)
		PG_RETURN_NULL();

	return d;
}

void
ExecEagerFreeWindow(WindowState * node)
{
	if (node->input_buffer != NULL)
		freeInputBuffer(node);

	freeFrameBuffers(node);
}

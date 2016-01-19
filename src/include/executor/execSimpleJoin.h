/*-------------------------------------------------------------------------
 *
 * execSimpleJoin.h
 *	  prototypes for execSimpleJoin.c
 *
 * Portions Copyright (c) 2015- Pivotal Inc.
 *-------------------------------------------------------------------------
 */
#ifndef EXECSIMPLEJOIN_H
#define EXECSIMPLEJOIN_H

#include "postgres.h"
#include "nodes/execnodes.h"
#include "executor/execAbstractReader.h"
#include "executor/execResilientJoin.h"

/* Does a SimpleJoin need a new outer tuple (i.e., previous outer tuple was already joined) */
#define SimpleJoin_NeedNewOuter(sjs) (SimpleJoinerStage_NeedOuter == sjs->joinerStage)

#define IS_SIMPLE_OUTER_JOIN(joinType) (joinType == JOIN_LEFT || joinType == JOIN_LASJ || joinType == JOIN_LASJ_NOTIN)

struct ResilientJoinState;
struct ResilientJoiner;
struct SimpleJoinState;

/* The function prototype for obtaining the next joined tuple from a SimpleJoiner */
typedef TupleTableSlot* (SimpleJoinJoinFunc)(struct SimpleJoinState *sjs);

/*
 * Joins a single outer tuples with a set of inner tuples as provided by an AbstractReader
 */
typedef struct SimpleJoinState
{
	/* its first field is NodeTag */
	JoinState js;

	/* The ResilientJoinState that this SimpleJoinState is part of */
	struct ResilientJoinState *resilientJoinState;

	/*
	 * The joiner that owns this simple joiner state. Only the owner
	 * gets to execute join. To change owner the previous owner must
	 * be finished
	 */
	ResilientJoiner *owner;

	/* The abstract tuple reader that provides the outer side */
	AbstractTupleReader *outerTupleReader;

	/* The abstract tuple reader that provides the inner side */
	AbstractTupleReader *innerTupleReader;

    /*
     * This is evaluated to verify if two tuples matches their hashable
     * column values. This is mostly for backward compatibility with
     * legacy hash join.
     */
	List           *hashqualclauses;

	/*
	 * A NULL inner tuple slot for outer joins (unmatched outer tuples
	 * would have null values for its inner columns)
	 */
	TupleTableSlot *nullInnerTupleSlot;

	/* Do we need to read a new outer tuple */
	bool needNewOuter;

	/*
	 * If outer join, and we couldn't match an outer tuple with any inner tuple, output
	 * a result tuple with null inner columns
	 */
	bool generateOuterJoinTuple;

	/*
	 * If it is either LASJ or LASJ-Notin (i.e., output only one UN-JOINED
	 * tuple per outer tuple)
	 */
	bool antiJoin;

	/* Should we only output only 1 joined tuple per-outer tuple? */
	bool semiJoin;

	/*
	 * Mark the join as done. The next join call would simply return empty tuple.
	 * Useful for terminating join early (e.g., during LASJ_NOTIN if a null tuple
	 * was found
	 */
	bool joinDone;
} SimpleJoinState;

extern void SimpleJoin_Setup(SimpleJoinState *sjs, struct ResilientJoinState *rjs, ResilientJoin *node, EState *estate, int eflags);
extern void SimpleJoin_BeginJoin(SimpleJoinState *sjs, ResilientJoiner *resilientJoiner,
		AbstractTupleReader *outerTupleReader, AbstractTupleReader *innerTupleReader);
extern TupleTableSlot * ExecuteSimpleJoin(SimpleJoinState *sjs);
extern void SimpleJoin_EndJoin(SimpleJoinState *sjs, ResilientJoiner *owner);
extern void SimpleJoin_End(SimpleJoinState *sjs);
extern int SimpleJoin_CountSlots(ResilientJoin *node);

#endif   /* EXECSIMPLEJOIN_H */

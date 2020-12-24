/*-------------------------------------------------------------------------
 *
 * visibility_summary.h
 *	  Functions to return human-readable summary of a tuple's visibility flags.
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 * src/include/utils/visibility_summary.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef VISIBILITY_SUMMARY_H
#define VISIBILITY_SUMMARY_H

/* Result codes for TupleTransactionStatus */
typedef enum TupleTransactionStatus
{
	TupleTransactionStatus_None,
	TupleTransactionStatus_Frozen,
	TupleTransactionStatus_HintCommitted,
	TupleTransactionStatus_HintAborted,
	TupleTransactionStatus_CLogInProgress,
	TupleTransactionStatus_CLogCommitted,
	TupleTransactionStatus_CLogAborted,
	TupleTransactionStatus_CLogSubCommitted,
} TupleTransactionStatus;

/* Result codes for TupleVisibilityStatus */
typedef enum TupleVisibilityStatus
{
	TupleVisibilityStatus_Unknown,
	TupleVisibilityStatus_InProgress,
	TupleVisibilityStatus_Aborted,
	TupleVisibilityStatus_Past,
	TupleVisibilityStatus_Now,
} TupleVisibilityStatus;

typedef struct TupleVisibilitySummary
{
	ItemPointerData				tid;
	int16						infomask;
	int16						infomask2;
	ItemPointerData				updateTid;
	TransactionId				xmin;
	TupleTransactionStatus      xminStatus;
	TransactionId				xmax;
	TupleTransactionStatus      xmaxStatus;
	CommandId					cid;			/* inserting or deleting command ID, or both */
	TupleVisibilityStatus		visibilityStatus;
} TupleVisibilitySummary;

// 0  gp_tid                    			TIDOID
// 1  gp_xmin                  			INT4OID
// 2  gp_xmin_status        			TEXTOID
// 3  gp_xmin_commit_distrib_id		TEXTOID
// 4  gp_xmax		          		INT4OID
// 5  gp_xmax_status       			TEXTOID
// 6  gp_xmax_distrib_id    			TEXTOID
// 7  gp_command_id	    			INT4OID
// 8  gp_infomask    	   			TEXTOID
// 9  gp_update_tid         			TIDOID
// 10 gp_visibility             			TEXTOID

extern void GetTupleVisibilitySummary(
	HeapTuple				tuple,
	TupleVisibilitySummary	*tupleVisibilitySummary);

extern void GetTupleVisibilitySummaryDatums(
	Datum		*values,
	bool		*nulls,
	TupleVisibilitySummary	*tupleVisibilitySummary);

extern char *GetTupleVisibilitySummaryString(
	TupleVisibilitySummary	*tupleVisibilitySummary);

#endif   /* VISIBILITY_SUMMARY_H */

/*-------------------------------------------------------------------------
 *
 * visibility_summary.c
 *	  Functions to return human-readable summary of a tuple's visibility flags.
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 * IDENTIFICATION
 *	  src/backend/utils/time/visibility_summary.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/clog.h"
#include "access/htup_details.h"
#include "access/distributedlog.h"
#include "access/transam.h"
#include "access/xact.h"
#include "cdb/cdbtm.h"
#include "cdb/cdbvars.h"
#include "lib/stringinfo.h"
#include "utils/builtins.h"
#include "utils/visibility_summary.h"

static char *
TupleTransactionStatus_Name(TupleTransactionStatus status)
{
	switch (status)
	{
		case TupleTransactionStatus_None:
			return "None";
		case TupleTransactionStatus_Frozen:
			return "Frozen";
		case TupleTransactionStatus_HintCommitted:
			return "Hint-Committed";
		case TupleTransactionStatus_HintAborted:
			return "Hint-Aborted";
		case TupleTransactionStatus_CLogInProgress:
			return "CLog-In-Progress";
		case TupleTransactionStatus_CLogCommitted:
			return "CLog-Committed";
		case TupleTransactionStatus_CLogAborted:
			return "CLog-Aborted";
		case TupleTransactionStatus_CLogSubCommitted:
			return "CLog-Sub-Committed";
		default:
			return "Unknown";
	}
}

static char *
TupleVisibilityStatus_Name(TupleVisibilityStatus status)
{
	switch (status)
	{
		case TupleVisibilityStatus_Unknown:
			return "Unknown";
		case TupleVisibilityStatus_InProgress:
			return "In-Progress";
		case TupleVisibilityStatus_Aborted:
			return "Aborted";
		case TupleVisibilityStatus_Past:
			return "Past";
		case TupleVisibilityStatus_Now:
			return "Now";
		default:
			return "Unknown";
	}
}

static TupleTransactionStatus GetTupleVisibilityCLogStatus(TransactionId xid)
{
	XidStatus	xidStatus;
	XLogRecPtr	lsn;

	xidStatus = TransactionIdGetStatus(xid, &lsn);
	switch (xidStatus)
	{
		case TRANSACTION_STATUS_IN_PROGRESS:
			return TupleTransactionStatus_CLogInProgress;
		case TRANSACTION_STATUS_COMMITTED:
			return TupleTransactionStatus_CLogCommitted;
		case TRANSACTION_STATUS_ABORTED:
			return TupleTransactionStatus_CLogAborted;
		case TRANSACTION_STATUS_SUB_COMMITTED:
			return TupleTransactionStatus_CLogSubCommitted;
		default:
			/* Never gets here.  XidStatus is only 2-bits. */
			return TupleTransactionStatus_None;
	}
}

void
GetTupleVisibilitySummary(HeapTuple tuple,
						  TupleVisibilitySummary *tupleVisibilitySummary)
{
	tupleVisibilitySummary->tid = tuple->t_self;
	tupleVisibilitySummary->infomask = tuple->t_data->t_infomask;
	tupleVisibilitySummary->infomask2 = tuple->t_data->t_infomask2;
	tupleVisibilitySummary->updateTid = tuple->t_data->t_ctid;

	tupleVisibilitySummary->xmin = HeapTupleHeaderGetXmin(tuple->t_data);
	if (!TransactionIdIsNormal(tupleVisibilitySummary->xmin))
	{
		if (tupleVisibilitySummary->xmin == FrozenTransactionId)
		{
			tupleVisibilitySummary->xminStatus = TupleTransactionStatus_Frozen;
		}
		else
		{
			tupleVisibilitySummary->xminStatus = TupleTransactionStatus_None;
		}
	}
	else if (tuple->t_data->t_infomask & HEAP_XMIN_COMMITTED)
	{
		tupleVisibilitySummary->xminStatus = TupleTransactionStatus_HintCommitted;
	}
	else if (tuple->t_data->t_infomask & HEAP_XMIN_INVALID)
	{
		tupleVisibilitySummary->xminStatus = TupleTransactionStatus_HintAborted;
	}
	else
	{
		tupleVisibilitySummary->xminStatus =
			GetTupleVisibilityCLogStatus(tupleVisibilitySummary->xmin);
	}
	tupleVisibilitySummary->xmax = HeapTupleHeaderGetRawXmax(tuple->t_data);
	if (!TransactionIdIsNormal(tupleVisibilitySummary->xmax))
	{
		if (tupleVisibilitySummary->xmax == FrozenTransactionId)
		{
			tupleVisibilitySummary->xmaxStatus = TupleTransactionStatus_Frozen;
		}
		else
		{
			tupleVisibilitySummary->xmaxStatus = TupleTransactionStatus_None;
		}
	}
	else if (tuple->t_data->t_infomask & HEAP_XMAX_COMMITTED)
	{
		tupleVisibilitySummary->xmaxStatus = TupleTransactionStatus_HintCommitted;
	}
	else if (tuple->t_data->t_infomask & HEAP_XMAX_INVALID)
	{
		tupleVisibilitySummary->xmaxStatus = TupleTransactionStatus_HintAborted;
	}
	else
	{
		tupleVisibilitySummary->xmaxStatus =
			GetTupleVisibilityCLogStatus(tupleVisibilitySummary->xmax);
	}

	tupleVisibilitySummary->cid =
		HeapTupleHeaderGetRawCommandId(tuple->t_data);

	/*
	 * Evaluate xmin and xmax status to produce overall visibility.
	 *
	 * UNDONE: Too simplistic?
	 */
	switch (tupleVisibilitySummary->xminStatus)
	{
		case TupleTransactionStatus_None:
		case TupleTransactionStatus_CLogSubCommitted:
			tupleVisibilitySummary->visibilityStatus = TupleVisibilityStatus_Unknown;
			break;

		case TupleTransactionStatus_CLogInProgress:
			tupleVisibilitySummary->visibilityStatus = TupleVisibilityStatus_InProgress;
			break;

		case TupleTransactionStatus_HintAborted:
		case TupleTransactionStatus_CLogAborted:
			tupleVisibilitySummary->visibilityStatus = TupleVisibilityStatus_Aborted;
			break;

		case TupleTransactionStatus_Frozen:
		case TupleTransactionStatus_HintCommitted:
		case TupleTransactionStatus_CLogCommitted:
			{
				switch (tupleVisibilitySummary->xmaxStatus)
				{
					case TupleTransactionStatus_None:
					case TupleTransactionStatus_Frozen:
					case TupleTransactionStatus_CLogInProgress:
					case TupleTransactionStatus_HintAborted:
					case TupleTransactionStatus_CLogAborted:
						tupleVisibilitySummary->visibilityStatus = TupleVisibilityStatus_Now;
						break;

					case TupleTransactionStatus_CLogSubCommitted:
						tupleVisibilitySummary->visibilityStatus = TupleVisibilityStatus_Unknown;
						break;

					case TupleTransactionStatus_HintCommitted:
					case TupleTransactionStatus_CLogCommitted:
						tupleVisibilitySummary->visibilityStatus = TupleVisibilityStatus_Past;
						break;

					default:
						elog(ERROR, "Unrecognized tuple transaction status: %d",
							 (int) tupleVisibilitySummary->xmaxStatus);
						tupleVisibilitySummary->visibilityStatus = TupleVisibilityStatus_Unknown;
						break;
				}
			}
			break;

		default:
			elog(ERROR, "Unrecognized tuple transaction status: %d",
				 (int) tupleVisibilitySummary->xminStatus);
			tupleVisibilitySummary->visibilityStatus = TupleVisibilityStatus_Unknown;
			break;
	}
}

static char *
GetTupleVisibilityDistribId(TransactionId xid,
							TupleTransactionStatus status)
{
	DistributedTransactionTimeStamp distribTimeStamp;
	DistributedTransactionId distribXid;

	switch (status)
	{
		case TupleTransactionStatus_None:
		case TupleTransactionStatus_Frozen:
		case TupleTransactionStatus_CLogInProgress:
		case TupleTransactionStatus_HintAborted:
		case TupleTransactionStatus_CLogAborted:
		case TupleTransactionStatus_CLogSubCommitted:
			return NULL;

		case TupleTransactionStatus_HintCommitted:
		case TupleTransactionStatus_CLogCommitted:
			if ((!IS_QUERY_DISPATCHER()) &&
				DistributedLog_CommittedCheck(xid,
											  &distribTimeStamp,
											  &distribXid))
			{
				char	   *distribId;

				distribId = palloc(TMGIDSIZE);
				dtxFormGID(distribId, distribTimeStamp, distribXid);
				return distribId;
			}
			else
			{
				return pstrdup("(info not avail)");
			}
			break;

		default:
			elog(ERROR, "Unrecognized tuple transaction status: %d",
				 (int) status);
			return NULL;
	}
}

static void
TupleVisibilityAddFlagName(StringInfoData *buf, int16 rawFlag,
						   char *flagName, bool *atLeastOne)
{
	if (rawFlag != 0)
	{
		if (*atLeastOne)
		{
			appendStringInfo(buf, ", ");
		}
		appendStringInfo(buf, "%s", flagName);
		*atLeastOne = true;
	}
}

static char *
GetTupleVisibilityInfoMaskSet(int16 infomask, int16 infomask2)
{
	StringInfoData buf;

	bool		atLeastOne;

	initStringInfo(&buf);
	appendStringInfo(&buf, "{");
	atLeastOne = false;

	TupleVisibilityAddFlagName(&buf, infomask & HEAP_COMBOCID, "HEAP_COMBOCID", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask & HEAP_XMAX_EXCL_LOCK, "HEAP_XMAX_EXCL_LOCK", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask & HEAP_XMAX_LOCK_ONLY, "HEAP_XMAX_LOCK_ONLY", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask & HEAP_XMIN_COMMITTED, "HEAP_XMIN_COMMITTED", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask & HEAP_XMIN_INVALID, "HEAP_XMIN_INVALID", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask & HEAP_XMAX_COMMITTED, "HEAP_XMAX_COMMITTED", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask & HEAP_XMAX_INVALID, "HEAP_XMAX_INVALID", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask & HEAP_XMAX_IS_MULTI, "HEAP_XMAX_IS_MULTI", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask & HEAP_UPDATED, "HEAP_UPDATED", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask & HEAP_MOVED_OFF, "HEAP_MOVED_OFF", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask & HEAP_MOVED_IN, "HEAP_MOVED_IN", &atLeastOne);

	TupleVisibilityAddFlagName(&buf, infomask2 & HEAP_XMIN_DISTRIBUTED_SNAPSHOT_IGNORE, "HEAP_XMIN_DISTRIBUTED_SNAPSHOT_IGNORE", &atLeastOne);
	TupleVisibilityAddFlagName(&buf, infomask2 & HEAP_XMAX_DISTRIBUTED_SNAPSHOT_IGNORE, "HEAP_XMAX_DISTRIBUTED_SNAPSHOT_IGNORE", &atLeastOne);

	appendStringInfo(&buf, "}");
	return buf.data;
}

/*  0  gp_tid                    			TIDOID */
/*  1  gp_xmin                  			INT4OID */
/*  2  gp_xmin_status        			TEXTOID */
/*  3  gp_xmin_commit_distrib_id		TEXTOID */
/*  4  gp_xmax		          		INT4OID */
/*  5  gp_xmax_status       			TEXTOID */
/*  6  gp_xmax_distrib_id    			TEXTOID */
/*  7  gp_command_id	    			INT4OID */
/*  8  gp_infomask    	   			TEXTOID */
/*  9  gp_update_tid         			TIDOID */
/*  10 gp_visibility             			TEXTOID */

void
GetTupleVisibilitySummaryDatums(Datum *values, bool *nulls,
								TupleVisibilitySummary *tupleVisibilitySummary)
{
	char	   *xminDistribId;
	char	   *xmaxDistribId;
	char	   *infoMaskSet;

	values[0] = ItemPointerGetDatum(&tupleVisibilitySummary->tid);
	values[1] = Int32GetDatum((int32) tupleVisibilitySummary->xmin);
	values[2] = CStringGetTextDatum(
		TupleTransactionStatus_Name(tupleVisibilitySummary->xminStatus));
	xminDistribId = GetTupleVisibilityDistribId(tupleVisibilitySummary->xmin,
												tupleVisibilitySummary->xminStatus);
	if (xminDistribId != NULL)
	{
		values[3] = CStringGetTextDatum(xminDistribId);
		pfree(xminDistribId);
	}
	else
	{
		nulls[3] = true;
	}
	values[4] = Int32GetDatum((int32) tupleVisibilitySummary->xmax);
	values[5] = CStringGetTextDatum(
		TupleTransactionStatus_Name(tupleVisibilitySummary->xmaxStatus));
	xmaxDistribId = GetTupleVisibilityDistribId(tupleVisibilitySummary->xmax,
												tupleVisibilitySummary->xmaxStatus);
	if (xmaxDistribId != NULL)
	{
		values[6] = CStringGetTextDatum(xmaxDistribId);
		pfree(xmaxDistribId);
	}
	else
	{
		nulls[6] = true;
	}
	values[7] = Int32GetDatum((int32) tupleVisibilitySummary->cid);
	infoMaskSet = GetTupleVisibilityInfoMaskSet(tupleVisibilitySummary->infomask,
												tupleVisibilitySummary->infomask2);
	values[8] = CStringGetTextDatum(infoMaskSet);
	pfree(infoMaskSet);
	values[9] = ItemPointerGetDatum(&tupleVisibilitySummary->updateTid);
	values[10] = CStringGetTextDatum(
	TupleVisibilityStatus_Name(tupleVisibilitySummary->visibilityStatus));
}

char *
GetTupleVisibilitySummaryString(TupleVisibilitySummary *tupleVisibilitySummary)
{
	StringInfoData buf;

	char	   *xminDistribId;
	char	   *xmaxDistribId;
	char	   *infoMaskSet;

	initStringInfo(&buf);
	appendStringInfo(&buf, "tid %s",
					 ItemPointerToString(&tupleVisibilitySummary->tid));
	appendStringInfo(&buf, ", xmin %u",
					 tupleVisibilitySummary->xmin);
	appendStringInfo(&buf, ", xmin_status '%s'",
					 TupleTransactionStatus_Name(tupleVisibilitySummary->xminStatus));

	xminDistribId = GetTupleVisibilityDistribId(tupleVisibilitySummary->xmin,
												tupleVisibilitySummary->xminStatus);
	if (xminDistribId != NULL)
	{
		appendStringInfo(&buf, ", xmin_commit_distrib_id '%s'",
						 xminDistribId);
		pfree(xminDistribId);
	}
	appendStringInfo(&buf, ", xmax %u",
					 tupleVisibilitySummary->xmax);
	appendStringInfo(&buf, ", xmax_status '%s'",
					 TupleTransactionStatus_Name(tupleVisibilitySummary->xmaxStatus));

	xmaxDistribId = GetTupleVisibilityDistribId(tupleVisibilitySummary->xmax,
												tupleVisibilitySummary->xmaxStatus);
	if (xmaxDistribId != NULL)
	{
		appendStringInfo(&buf, ", xmax_commit_distrib_id '%s'",
						 xmaxDistribId);
		pfree(xmaxDistribId);
	}
	appendStringInfo(&buf, ", command_id %u",
					 tupleVisibilitySummary->cid);
	infoMaskSet = GetTupleVisibilityInfoMaskSet(tupleVisibilitySummary->infomask,
												tupleVisibilitySummary->infomask2);
	appendStringInfo(&buf, ", infomask '%s'",
					 infoMaskSet);
	pfree(infoMaskSet);
	appendStringInfo(&buf, ", update_tid %s",
					 ItemPointerToString(&tupleVisibilitySummary->updateTid));
	appendStringInfo(&buf, ", visibility '%s'",
					 TupleVisibilityStatus_Name(tupleVisibilitySummary->visibilityStatus));

	return buf.data;
}

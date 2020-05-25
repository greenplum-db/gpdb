/*-------------------------------------------------------------------------
 *
 * cdbdistributedxid.c
 *		Function to return maximum distributed transaction id.
 *
 * IDENTIFICATION
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "funcapi.h"
#include "utils/builtins.h"
#include "cdb/cdbtm.h"

Datum		gp_distributed_xid(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(gp_distributed_xid);
Datum
gp_distributed_xid(PG_FUNCTION_ARGS pg_attribute_unused())
{
	DistributedTransactionId xid = getDistributedTransactionId();

	PG_RETURN_XID(xid);

}

/*
 * DistributedTxnIdWithTsPrecedes --- is id1+ts1 logically < id2+ts2?
 */
bool
DistributedTxnIdWithTsPrecedes
(DistributedTransactionId id1, DistributedTransactionTimeStamp ts1,
DistributedTransactionId id2, DistributedTransactionTimeStamp ts2)
{
	Assert(gp_enable_dxid_wraparound);

	if (ts1 < ts2)
		return true;
	else if (ts1 == ts2)
	{
		if (id1 < id2)
			return true;
		else
			return false;
	}
	else
		return false;
}

/*
 * DistributedTxnIdWithTsPrecedesOrEquals --- is id1+ts1 logically <= id2+ts2?
 */
bool
DistributedTxnIdWithTsPrecedesOrEquals
(DistributedTransactionId id1, DistributedTransactionTimeStamp ts1,
DistributedTransactionId id2, DistributedTransactionTimeStamp ts2)
{
	Assert(gp_enable_dxid_wraparound);

	if (ts1 < ts2)
		return true;
	else if (ts1 == ts2)
	{
		if (id1 <= id2)
			return true;
		else
			return false;
	}
	else
		return false;
}

/*
 * DistributedTxnIdWithTsFollows --- is id1+ts1 logically > id2+ts2?
 */
bool
DistributedTxnIdWithTsFollows
(DistributedTransactionId id1, DistributedTransactionTimeStamp ts1,
DistributedTransactionId id2, DistributedTransactionTimeStamp ts2)
{
	Assert(gp_enable_dxid_wraparound);

	if (ts1 > ts2)
		return true;
	else if (ts1 == ts2)
	{
		if (id1 > id2)
			return true;
		else
			return false;
	}
	else
		return false;
}

/*
 * DistributedTxnIdWithTsFollowsOrEquals --- is id1+ts1 logically >= id2+ts2?
 */
bool
DistributedTxnIdWithTsFollowsOrEquals
(DistributedTransactionId id1, DistributedTransactionTimeStamp ts1,
DistributedTransactionId id2, DistributedTransactionTimeStamp ts2)
{
	Assert(gp_enable_dxid_wraparound);

	if (ts1 > ts2)
		return true;
	else if (ts1 == ts2)
	{
		if (id1 >= id2)
			return true;
		else
			return false;
	}
	else
		return false;
}

/*
 * DistributedTxnIdWithTsAdd --- increase a given timestamp and dxid
 */
void
DistributedTxnIdWithTsAdd
(DistributedTransactionId *id, DistributedTransactionTimeStamp *ts,
DistributedTransactionId incre_id, DistributedTransactionTimeStamp incre_ts)
{
	Assert(gp_enable_dxid_wraparound);

	DistributedTransactionId new_id = *id + incre_id;
	if (new_id >= *id)
		*ts = *ts + incre_ts;
	else
		*ts = *ts + incre_ts + 1;
	*id = new_id;
}

/*
 * DistributedTransactionIdPrecedes --- is id1 logically < id2?
 */
bool
DistributedTransactionIdPrecedes(DistributedTransactionId id1, DistributedTransactionId id2)
{
	int32 diff;
	if (gp_enable_dxid_wraparound)
	{
		diff = (int32) (id1 - id2);
		return (diff < 0);
	}
	else
		return (id1 < id2);
}

/*
 * DistributedTransactionIdPrecedesOrEquals --- is id1 logically <= id2?
 */
bool
DistributedTransactionIdPrecedesOrEquals(DistributedTransactionId id1, DistributedTransactionId id2)
{
	int32 diff;

	if (gp_enable_dxid_wraparound)
	{
		diff = (int32) (id1 - id2);
		return (diff <= 0);
	}
	else
		return (id1 <= id2);
}

/*
 * DistributedTransactionIdFollows --- is id1 logically > id2?
  */
bool
DistributedTransactionIdFollows(DistributedTransactionId id1, DistributedTransactionId id2)
{
	int32 diff;

	if (gp_enable_dxid_wraparound)
	{
		diff = (int32) (id1 - id2);
		return (diff > 0);
	}
	else
		return (id1 > id2);
}

/*
 * DistributedTransactionIdFollowsOrEquals --- is id1 logically >= id2?
 */
bool
DistributedTransactionIdFollowsOrEquals(DistributedTransactionId id1, DistributedTransactionId id2)
{
	int32 diff;

	if (gp_enable_dxid_wraparound)
	{
		diff = (int32) (id1 - id2);
		return (diff >= 0);
	}
	else
		return (id1 >= id2);
}

/*
 * This is used to compute the timestamp for a given dxid based on the pair of dxid and timestamp
 * when dxid wraparound is enabled.
 */
DistributedTransactionTimeStamp
getDxidTimestamp(DistributedTransactionId dxid, DistributedTransactionId refDxid,
				 DistributedTransactionTimeStamp refTs)
{
	if (dxid == refDxid)
		return refTs;
	/* logically, dxid < refDxid */
	else if (DistributedTransactionIdPrecedes(dxid, refDxid))
	{
		/*
		 * logically, LastDXID < dxid < refDxid
		 * logically, dxid < refDxid  < lastDXID
		 */
		if (DistributedTransactionIdPrecedes(LastDistributedTransactionId, dxid) ||
			DistributedTransactionIdPrecedes(refDxid, LastDistributedTransactionId))
			return refTs;
		/* logically, dxid < lastDXID < refDxid */
		else
			return refTs - 1;
	}
	/* logically, refDxid < dxid */
	else
	{
		/*
		 * logically, LastDXID < refDxid < dxid
		 * logically, refDxid < dxid < lastDXID
		 */
		if (DistributedTransactionIdPrecedes(LastDistributedTransactionId, refDxid) ||
			DistributedTransactionIdPrecedes(dxid, LastDistributedTransactionId))
			return refTs;
		/* logically, refDxid < lastDXID < dxid */
		else
			return refTs + 1;
	}
}

#include "postgres.h"
#include "funcapi.h"

#include "access/clog.h"
#include "access/distributedlog.h"
#include "access/subtrans.h"
#include "access/transam.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

extern Datum spoof_next_xid(PG_FUNCTION_ARGS);
extern Datum get_oldest_xid(PG_FUNCTION_ARGS);
extern Datum get_next_xid(PG_FUNCTION_ARGS);
extern Datum get_vac_limit(PG_FUNCTION_ARGS);
extern Datum get_warn_limit(PG_FUNCTION_ARGS);
extern Datum get_stop_limit(PG_FUNCTION_ARGS);
extern Datum get_wrap_limit(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(spoof_next_xid);
Datum
spoof_next_xid(PG_FUNCTION_ARGS)
{
	TransactionId desiredXid = PG_GETARG_UINT32(0);
	TransactionId oldXid = ShmemVariableCache->nextXid;
	ShmemVariableCache->nextXid = desiredXid;

	/*
	 * If we're raising the xid, the intent is presumably to cross some
	 * threshold and make assertions about expected behavior.
	 * On the other hand, lowering the xid is meant to be a tear down of
	 * a completed test case. Because of this distinction, only when
	 * we're raising the xid, do we take extra precaution to zero out
	 * the new pg_clog/pg_subtrans/pg_distributedlog files. (We don't
	 * want to zero out existing files...)
	 */
	if (TransactionIdFollows(desiredXid, oldXid))
	{
		/*
		 * The nature of xid arithmetic is such that we only bother zeroing out
		 * new pages of transaction files when we've crossed page boundaries.
		 * So, here we fool the following routines into zeroing out the desired
		 * pages of transaction metadata by lowering the input xid to the first
		 * of its corresponding page.
		 */
#define CLOG_XACTS_PER_BYTE 4
#define CLOG_XACTS_PER_PAGE (BLCKSZ * CLOG_XACTS_PER_BYTE)
#define TransactionIdToPgIndex(xid) ((xid) % (TransactionId) CLOG_XACTS_PER_PAGE)
		ExtendCLOG(desiredXid - TransactionIdToPgIndex(desiredXid));
#define SUBTRANS_XACTS_PER_PAGE (BLCKSZ / sizeof(SubTransData))
#define TransactionIdToEntry(xid) ((xid) % (uint32) SUBTRANS_XACTS_PER_PAGE)
		ExtendSUBTRANS(desiredXid - TransactionIdToEntry(desiredXid));
#undef TransactionIdToEntry
#define ENTRIES_PER_PAGE (BLCKSZ / sizeof(DistributedLogEntry))
#define TransactionIdToEntry(localXid) ((localXid) % (TransactionId) ENTRIES_PER_PAGE)
		DistributedLog_Extend(desiredXid - TransactionIdToEntry(desiredXid));
	}

	PG_RETURN_XID(oldXid);
}

PG_FUNCTION_INFO_V1(get_oldest_xid);
Datum
get_oldest_xid(PG_FUNCTION_ARGS)
{
	PG_RETURN_XID(ShmemVariableCache->oldestXid);
}

PG_FUNCTION_INFO_V1(get_next_xid);
Datum
get_next_xid(PG_FUNCTION_ARGS)
{
	PG_RETURN_XID(ShmemVariableCache->nextXid);
}

PG_FUNCTION_INFO_V1(get_vac_limit);
Datum
get_vac_limit(PG_FUNCTION_ARGS)
{
	PG_RETURN_XID(ShmemVariableCache->xidVacLimit);
}

PG_FUNCTION_INFO_V1(get_warn_limit);
Datum
get_warn_limit(PG_FUNCTION_ARGS)
{
	PG_RETURN_XID(ShmemVariableCache->xidWarnLimit);
}

PG_FUNCTION_INFO_V1(get_stop_limit);
Datum
get_stop_limit(PG_FUNCTION_ARGS)
{
	PG_RETURN_XID(ShmemVariableCache->xidStopLimit);
}

PG_FUNCTION_INFO_V1(get_wrap_limit);
Datum
get_wrap_limit(PG_FUNCTION_ARGS)
{
	PG_RETURN_XID(ShmemVariableCache->xidWrapLimit);
}

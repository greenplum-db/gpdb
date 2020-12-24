/*-------------------------------------------------------------------------
 *
 * cdbtmutils.c
 *	  Provides routines for help performing distributed transaction
 *    management
 *
 * Unlike cdbtm.c, this file deals mainly with packing and unpacking
 * structures, converting values to strings, etc.
 *
 * Portions Copyright (c) 2005-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbtmutils.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "cdb/cdbtm.h"
#include "cdb/cdbvars.h"
#include "access/xact.h"

/*
 * Crack open the gid to get the DTM start time and distributed
 * transaction id.
 */
void
dtxCrackOpenGid(
				const char *gid,
				DistributedTransactionTimeStamp *distribTimeStamp,
				DistributedTransactionId *distribXid)
{
	int			itemsScanned;

	itemsScanned = sscanf(gid, "%u-%u", distribTimeStamp, distribXid);
	if (itemsScanned != 2)
	{
		/*
		 * Returning without an error here allows tests inheritied from
		 * upstream PostgreSQL to run without errors.  These tests execute
		 * PREPARE TRANSACTION command with a GID that doesn't conform to the
		 * Greenplum specific format.  Note that DTM messages sent from QD
		 * cannot be processed in utility mode.  Therefore, we can safely
		 * allow non-Greenplum GIDs only in utility mode.
		 */
		if (Gp_role == GP_ROLE_UTILITY)
			*distribTimeStamp = *distribXid = 0;
		else
			elog(ERROR, "Bad distributed transaction identifier \"%s\"", gid);
	}
}

void
dtxFormGID(char *gid, DistributedTransactionTimeStamp tstamp, DistributedTransactionId gxid)
{
	sprintf(gid, "%u-%.10u", tstamp, gxid);
	/* gxid is unsigned int32 and its max string length is 10 */
	Assert(strlen(gid) < TMGIDSIZE);
}

char *
DtxStateToString(DtxState state)
{
	switch (state)
	{
		case DTX_STATE_NONE:
			return "None";
		case DTX_STATE_ACTIVE_DISTRIBUTED:
			return "Active Distributed";
		case DTX_STATE_ONE_PHASE_COMMIT:
			return "One Phase Commit (Before Notifying)";
		case DTX_STATE_NOTIFYING_ONE_PHASE_COMMIT:
			return "Notifying One Phase";
		case DTX_STATE_PREPARING:
			return "Preparing";
		case DTX_STATE_PREPARED:
			return "Prepared";
		case DTX_STATE_INSERTING_COMMITTED:
			return "Inserting Committed";
		case DTX_STATE_INSERTED_COMMITTED:
			return "Inserted Committed";
		case DTX_STATE_NOTIFYING_COMMIT_PREPARED:
			return "Notifying Commit Prepared";
		case DTX_STATE_INSERTING_FORGET_COMMITTED:
			return "Inserting Forget Committed";
		case DTX_STATE_INSERTED_FORGET_COMMITTED:
			return "Inserted Forget Committed";
		case DTX_STATE_NOTIFYING_ABORT_NO_PREPARED:
			return "Notifying Abort (No Prepared)";
		case DTX_STATE_NOTIFYING_ABORT_SOME_PREPARED:
			return "Notifying Abort (Some Prepared)";
		case DTX_STATE_NOTIFYING_ABORT_PREPARED:
			return "Notifying Abort Prepared";
		case DTX_STATE_RETRY_COMMIT_PREPARED:
			return "Retry Commit Prepared";
		case DTX_STATE_RETRY_ABORT_PREPARED:
			return "Retry Abort Prepared";
		default:
			return "Unknown";
	}
}

char *
DtxProtocolCommandToString(DtxProtocolCommand command)
{
	switch (command)
	{
		case DTX_PROTOCOL_COMMAND_NONE:
			return "None";
		case DTX_PROTOCOL_COMMAND_ABORT_NO_PREPARED:
			return "Distributed Abort (No Prepared)";
		case DTX_PROTOCOL_COMMAND_PREPARE:
			return "Distributed Prepare";
		case DTX_PROTOCOL_COMMAND_ABORT_SOME_PREPARED:
			return "Distributed Abort (Some Prepared)";
		case DTX_PROTOCOL_COMMAND_COMMIT_ONEPHASE:
			return "Distributed Commit (one-phase)";
		case DTX_PROTOCOL_COMMAND_COMMIT_PREPARED:
			return "Distributed Commit Prepared";
		case DTX_PROTOCOL_COMMAND_ABORT_PREPARED:
			return "Distributed Abort Prepared";
		case DTX_PROTOCOL_COMMAND_RETRY_COMMIT_PREPARED:
			return "Retry Distributed Commit Prepared";
		case DTX_PROTOCOL_COMMAND_RETRY_ABORT_PREPARED:
			return "Retry Distributed Abort Prepared";
		case DTX_PROTOCOL_COMMAND_RECOVERY_COMMIT_PREPARED:
			return "Recovery Commit Prepared";
		case DTX_PROTOCOL_COMMAND_RECOVERY_ABORT_PREPARED:
			return "Recovery Abort Prepared";
		case DTX_PROTOCOL_COMMAND_SUBTRANSACTION_BEGIN_INTERNAL:
			return " Begin Internal Subtransaction";
		case DTX_PROTOCOL_COMMAND_SUBTRANSACTION_RELEASE_INTERNAL:
			return "Release Current Subtransaction";
		case DTX_PROTOCOL_COMMAND_SUBTRANSACTION_ROLLBACK_INTERNAL:
			return "Rollback Current Subtransaction";
	}

	return "Unknown";
}

char *
DtxContextToString(DtxContext context)
{
	switch (context)
	{
		case DTX_CONTEXT_LOCAL_ONLY:
			return "Local Only";
		case DTX_CONTEXT_QD_DISTRIBUTED_CAPABLE:
			return "Master Distributed-Capable";
		case DTX_CONTEXT_QD_RETRY_PHASE_2:
			return "Master Retry Phase 2";
		case DTX_CONTEXT_QE_ENTRY_DB_SINGLETON:
			return "Segment Entry DB Singleton";
		case DTX_CONTEXT_QE_AUTO_COMMIT_IMPLICIT:
			return "Segment Auto-Commit Implicit";
		case DTX_CONTEXT_QE_TWO_PHASE_EXPLICIT_WRITER:
			return "Segment Two-Phase Explicit Writer";
		case DTX_CONTEXT_QE_TWO_PHASE_IMPLICIT_WRITER:
			return "Segment Two-Phase Implicit Writer";
		case DTX_CONTEXT_QE_READER:
			return "Segment Reader";
		case DTX_CONTEXT_QE_PREPARED:
			return "Segment Prepared";
		case DTX_CONTEXT_QE_FINISH_PREPARED:
			return "Segment Finish Prepared";
		default:
			return "Unknown";
	}
}

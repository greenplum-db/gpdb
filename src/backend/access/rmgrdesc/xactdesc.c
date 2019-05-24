/*-------------------------------------------------------------------------
 *
 * xactdesc.c
 *	  rmgr descriptor routines for access/transam/xact.c
 *
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/rmgrdesc/xactdesc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/xact.h"
#include "catalog/catalog.h"
#include "storage/dbdirnode.h"
#include "storage/sinval.h"
#include "utils/timestamp.h"


static char*
xact_desc_commit(StringInfo buf, xl_xact_commit *xlrec)
{
	int			i;
	TransactionId *subxacts;
	SharedInvalidationMessage *msgs;
	DbDirNode *deldbs;

	subxacts = (TransactionId *) &xlrec->xnodes[xlrec->nrels];
	msgs = (SharedInvalidationMessage *) &subxacts[xlrec->nsubxacts];
	deldbs = (DbDirNode *) &(msgs[xlrec->nmsgs]);

	appendStringInfoString(buf, timestamptz_to_str(xlrec->xact_time));

	if (xlrec->nrels > 0)
	{
		appendStringInfoString(buf, "; rels:");
		for (i = 0; i < xlrec->nrels; i++)
		{
			BackendId  backendId = xlrec-> xnodes[i].isTempRelation ?
								  TempRelBackendId : InvalidBackendId;
			char	   *path = relpathbackend(xlrec->xnodes[i].node,
											  backendId,
											  MAIN_FORKNUM);

			appendStringInfo(buf, " %s", path);
			pfree(path);
		}
	}
	if (xlrec->nsubxacts > 0)
	{
		appendStringInfoString(buf, "; subxacts:");
		for (i = 0; i < xlrec->nsubxacts; i++)
			appendStringInfo(buf, " %u", subxacts[i]);
	}
	if (xlrec->nmsgs > 0)
	{
		if (XactCompletionRelcacheInitFileInval(xlrec->xinfo))
			appendStringInfo(buf, "; relcache init file inval dbid %u tsid %u",
							 xlrec->dbId, xlrec->tsId);

		appendStringInfoString(buf, "; inval msgs:");
		for (i = 0; i < xlrec->nmsgs; i++)
		{
			SharedInvalidationMessage *msg = &msgs[i];

			if (msg->id >= 0)
				appendStringInfo(buf, " catcache %d", msg->id);
			else if (msg->id == SHAREDINVALCATALOG_ID)
				appendStringInfo(buf, " catalog %u", msg->cat.catId);
			else if (msg->id == SHAREDINVALRELCACHE_ID)
				appendStringInfo(buf, " relcache %u", msg->rc.relId);
			/* not expected, but print something anyway */
			else if (msg->id == SHAREDINVALSMGR_ID)
				appendStringInfoString(buf, " smgr");
			/* not expected, but print something anyway */
			else if (msg->id == SHAREDINVALRELMAP_ID)
				appendStringInfoString(buf, " relmap");
			else if (msg->id == SHAREDINVALSNAPSHOT_ID)
				appendStringInfo(buf, " snapshot %u", msg->sn.relId);
			else
				appendStringInfo(buf, " unknown id %d", msg->id);
		}
	}
	if (xlrec->distribTimeStamp != 0 || xlrec->distribXid != InvalidDistributedTransactionId)
		appendStringInfo(buf, " gid = %u-%.10u", xlrec->distribTimeStamp, xlrec->distribXid);
	if (xlrec->ndeldbs > 0)
	{
		appendStringInfoString(buf, "; deldbs:");
		for (i = 0; i < xlrec->ndeldbs; i++)
		{
			char *path =
					 GetDatabasePath(deldbs[i].database, deldbs[i].tablespace);

			appendStringInfo(buf, " %s", path);
			pfree(path);
		}
	}

	/*
-	 * MPP: Return end of regular commit information.
	 */
	return (char *) &deldbs[xlrec->ndeldbs];
}

static void
xact_desc_distributed_commit(StringInfo buf, xl_xact_commit *xlrec)
{
	TMGXACT_LOG *gxact_log;

	/*
	 * We put the global transaction information last, so call the regular xact
	 * commit routine.
	 */
	gxact_log = (TMGXACT_LOG *) xact_desc_commit(buf, xlrec);

	appendStringInfo(buf, " gid = %s, gxid = %u",
					 gxact_log->gid, gxact_log->gxid);
}

static void
xact_desc_distributed_forget(StringInfo buf, xl_xact_distributed_forget *xlrec)
{
	appendStringInfo(buf, " gid = %s, gxid = %u",
					 xlrec->gxact_log.gid, xlrec->gxact_log.gxid);
}


static void
xact_desc_commit_compact(StringInfo buf, xl_xact_commit_compact *xlrec)
{
	int			i;

	appendStringInfoString(buf, timestamptz_to_str(xlrec->xact_time));

	if (xlrec->nsubxacts > 0)
	{
		appendStringInfoString(buf, "; subxacts:");
		for (i = 0; i < xlrec->nsubxacts; i++)
			appendStringInfo(buf, " %u", xlrec->subxacts[i]);
	}
}

static void
xact_desc_abort(StringInfo buf, xl_xact_abort *xlrec)
{
	int				i;
	TransactionId	*xacts;
	DbDirNode		*deldbs;

	xacts = (TransactionId *) &xlrec->xnodes[xlrec->nrels];
	deldbs = (DbDirNode *) &(xacts[xlrec->nsubxacts]);

	appendStringInfoString(buf, timestamptz_to_str(xlrec->xact_time));
	if (xlrec->nrels > 0)
	{
		appendStringInfoString(buf, "; rels:");
		for (i = 0; i < xlrec->nrels; i++)
		{
			BackendId  backendId = xlrec-> xnodes[i].isTempRelation ?
								  TempRelBackendId : InvalidBackendId;
			char	   *path = relpathbackend(xlrec->xnodes[i].node,
											  backendId,
											  MAIN_FORKNUM);

			appendStringInfo(buf, " %s", path);
			pfree(path);
		}
	}
	if (xlrec->nsubxacts > 0)
	{
		appendStringInfoString(buf, "; subxacts:");
		for (i = 0; i < xlrec->nsubxacts; i++)
			appendStringInfo(buf, " %u", xacts[i]);
	}
	if (xlrec->ndeldbs > 0)
	{
		appendStringInfoString(buf, "; deldbs:");
		for (i = 0; i < xlrec->ndeldbs; i++)
		{
			char *path =
					 GetDatabasePath(deldbs[i].database, deldbs[i].tablespace);

			appendStringInfo(buf, " %s", path);
			pfree(path);
		}
	}
}

static void
xact_desc_assignment(StringInfo buf, xl_xact_assignment *xlrec)
{
	int			i;

	appendStringInfoString(buf, "subxacts:");

	for (i = 0; i < xlrec->nsubxacts; i++)
		appendStringInfo(buf, " %u", xlrec->xsub[i]);
}

void
xact_desc(StringInfo buf, XLogRecord *record)
{
	uint8		info = record->xl_info & ~XLR_INFO_MASK;
	char		*rec = XLogRecGetData(record);

	if (info == XLOG_XACT_COMMIT_COMPACT)
	{
		xl_xact_commit_compact *xlrec = (xl_xact_commit_compact *) rec;

		appendStringInfoString(buf, "commit: ");
		xact_desc_commit_compact(buf, xlrec);
	}
	else if (info == XLOG_XACT_COMMIT)
	{
		xl_xact_commit *xlrec = (xl_xact_commit *) rec;

		appendStringInfoString(buf, "commit: ");
		xact_desc_commit(buf, xlrec);
	}
	else if (info == XLOG_XACT_ABORT)
	{
		xl_xact_abort *xlrec = (xl_xact_abort *) rec;

		appendStringInfoString(buf, "abort: ");
		xact_desc_abort(buf, xlrec);
	}
	else if (info == XLOG_XACT_PREPARE)
	{
		appendStringInfoString(buf, "prepare");
	}
	else if (info == XLOG_XACT_COMMIT_PREPARED)
	{
		xl_xact_commit_prepared *xlrec = (xl_xact_commit_prepared *) rec;

		appendStringInfo(buf, "commit prepared %u: ", xlrec->xid);
		xact_desc_commit(buf, &xlrec->crec);

		appendStringInfo(buf, " gid = %u-%.10u", xlrec->distribTimeStamp, xlrec->distribXid);
		appendStringInfo(buf, " gxid = %u", xlrec->distribXid);
	}
	else if (info == XLOG_XACT_ABORT_PREPARED)
	{
		xl_xact_abort_prepared *xlrec = (xl_xact_abort_prepared *) rec;

		appendStringInfo(buf, "abort prepared %u: ", xlrec->xid);
		xact_desc_abort(buf, &xlrec->arec);
	}
	else if (info == XLOG_XACT_ASSIGNMENT)
	{
		xl_xact_assignment *xlrec = (xl_xact_assignment *) rec;

		/*
		 * Note that we ignore the WAL record's xid, since we're more
		 * interested in the top-level xid that issued the record and which
		 * xids are being reported here.
		 */
		appendStringInfo(buf, "xid assignment xtop %u: ", xlrec->xtop);
		xact_desc_assignment(buf, xlrec);
	}
	else if (info == XLOG_XACT_DISTRIBUTED_COMMIT)
	{
		xl_xact_commit *xlrec = (xl_xact_commit *) rec;

		appendStringInfo(buf, "distributed commit ");
		xact_desc_distributed_commit(buf, xlrec);
	}
	else if (info == XLOG_XACT_DISTRIBUTED_FORGET)
	{
		xl_xact_distributed_forget *xlrec = (xl_xact_distributed_forget *) rec;

		appendStringInfo(buf, "distributed forget ");
		xact_desc_distributed_forget(buf, xlrec);
	}
	else
		appendStringInfoString(buf, "UNKNOWN");
}

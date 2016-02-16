/*-------------------------------------------------------------------------
 *
 * cdbslicetable.c
 *	  Slice table management
 *
 * Copyright (c) 2005-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <unistd.h>				/* getpid() */
#include <pthread.h>
#include <limits.h>

#include "gp-libpq-fe.h"
#include "miscadmin.h"			/* MyDatabaseId */
#include "storage/proc.h"		/* MyProc */
#include "storage/ipc.h"
#include "utils/memutils.h"

#include "catalog/namespace.h"
#include "commands/variable.h"
#include "nodes/execnodes.h"	/* CdbProcess, Slice, SliceTable */
#include "postmaster/postmaster.h"
#include "tcop/tcopprot.h"
#include "utils/portal.h"
#include "tcop/pquery.h"

#include "cdb/cdbconn.h"		/* SegmentDatabaseDescriptor */
#include "cdb/cdbfts.h"
#include "cdb/cdbdisp.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbslicetable.h"
#include "cdb/cdbtm.h"			/* discardDtxTransaction() */
#include "cdb/cdbutil.h"		/* CdbComponentDatabaseInfo */
#include "cdb/cdbvars.h"		/* Gp_role, etc. */
#include "storage/bfz.h"
#include "gp-libpq-fe.h"
#include "gp-libpq-int.h"
#include "libpq/libpq-be.h"
#include "libpq/ip.h"

#include "utils/guc_tables.h"

/*
 * Points to the result of getCdbComponentDatabases()
 */
CdbComponentDatabases *cdb_component_dbs = NULL;

/**
 * @param directDispatch may be null
 */
List *
getCdbProcessList(Gang *gang, int sliceIndex, DirectDispatchInfo *directDispatch)
{
	int			i;
	List	   *list = NIL;

	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(gang != NULL);

	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
		elog(LOG, "getCdbProcessList slice%d gangtype=%d gangsize=%d",
			 sliceIndex, gang->type, gang->size);

	if (gang != NULL && gang->type != GANGTYPE_UNALLOCATED)
	{
		CdbComponentDatabaseInfo *qeinfo;
		int			listsize = 0;

		for (i = 0; i < gang->size; i++)
		{
			CdbProcess *process;
			SegmentDatabaseDescriptor *segdbDesc;
			bool includeThisProcess = true;

			segdbDesc = &gang->db_descriptors[i];
			if (directDispatch != NULL && directDispatch->isDirectDispatch)
			{
				ListCell *cell;

				includeThisProcess = false;
				foreach(cell, directDispatch->contentIds)
				{
					if (lfirst_int(cell) == segdbDesc->segindex)
					{
						includeThisProcess = true;
						break;
					}
				}
			}

			/*
			 * We want the n-th element of the list to be the segDB that handled content n.
			 * And if no segDb is available (down mirror), we want it to be null.
			 *
			 * But the gang structure element n is not necessarily the guy who handles content n,
			 * so we need to skip some slots.
			 *
			 *
			 * We don't do this for reader 1-gangs
			 *
			 */
			if (gang->size > 1 ||
				gang->type == GANGTYPE_PRIMARY_WRITER)
			{
				while (segdbDesc->segindex > listsize)
				{
					list = lappend(list, NULL);
					listsize++;
				}
			}

			if (!includeThisProcess)
			{
				list = lappend(list, NULL);
				listsize++;
				continue;
			}

			process = (CdbProcess *) makeNode(CdbProcess);
			qeinfo = segdbDesc->segment_database_info;

			if (qeinfo == NULL)
			{
				elog(ERROR, "required segment is unavailable");
			}
			else if (qeinfo->hostip == NULL)
			{
				elog(ERROR, "required segment IP is unavailable");
			}

			process->listenerAddr = pstrdup(qeinfo->hostip);

			if (Gp_interconnect_type == INTERCONNECT_TYPE_UDPIFC || Gp_interconnect_type == INTERCONNECT_TYPE_UDP)
				process->listenerPort = (segdbDesc->motionListener >> 16) & 0x0ffff;
			else
				process->listenerPort = (segdbDesc->motionListener & 0x0ffff);

			process->pid = segdbDesc->backendPid;
			process->contentid = segdbDesc->segindex;

			if (gp_log_gang >= GPVARS_VERBOSITY_VERBOSE || DEBUG4 >= log_min_messages)
				elog(LOG, "Gang assignment (gang_id %d): slice%d seg%d %s:%d pid=%d",
					 gang->gang_id,
					 sliceIndex,
					 process->contentid,
					 process->listenerAddr,
					 process->listenerPort,
					 process->pid);

			list = lappend(list, process);
			listsize++;
		}

		insist_log( ! (gang->type == GANGTYPE_PRIMARY_WRITER &&
			listsize < getgpsegmentCount()),
			"master segworker group smaller than number of segments");

		if (gang->size > 1 ||
			gang->type == GANGTYPE_PRIMARY_WRITER)
		{
			while (listsize < getgpsegmentCount())
			{
				list = lappend(list, NULL);
				listsize++;
			}
		}
		Assert(listsize == 1 || listsize == getgpsegmentCount());
	}

	return list;
}

/*
 * getCdbProcessForQD:	Manufacture a CdbProcess representing the QD,
 * as if it were a worker from the executor factory.
 *
 * NOTE: Does not support multiple (mirrored) QDs.
 */
List *
getCdbProcessesForQD(int isPrimary)
{
	List	   *list = NIL;

	CdbComponentDatabaseInfo *qdinfo;
	CdbProcess *proc;

	Assert(Gp_role == GP_ROLE_DISPATCH);

	if (!isPrimary)
	{
		elog(FATAL, "getCdbProcessesForQD: unsupported request for master mirror process");
	}

	if (cdb_component_dbs == NULL)
	{
		cdb_component_dbs = getCdbComponentDatabases();
		if (cdb_component_dbs == NULL)
			elog(ERROR, PACKAGE_NAME " schema not populated");
	}

	qdinfo = &(cdb_component_dbs->entry_db_info[0]);

	Assert(qdinfo->segindex == -1);
	Assert(SEGMENT_IS_ACTIVE_PRIMARY(qdinfo));
	Assert(qdinfo->hostip != NULL);

	proc = makeNode(CdbProcess);
	/*
	 * Set QD listener address to NULL. This
	 * will be filled during starting up outgoing
	 * interconnect connection.
	 */
	proc->listenerAddr = NULL;
	if (Gp_interconnect_type == INTERCONNECT_TYPE_UDPIFC || Gp_interconnect_type == INTERCONNECT_TYPE_UDP)
		proc->listenerPort = (Gp_listener_port >> 16) & 0x0ffff;
	else
		proc->listenerPort = (Gp_listener_port & 0x0ffff);
	proc->pid = MyProcPid;
	proc->contentid = -1;

	/*
	 * freeCdbComponentDatabases(cdb_component_dbs);
	 */
	list = lappend(list, proc);
	return list;
}

CdbComponentDatabases *
getComponentDatabases(void)
{
	Assert(Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_UTILITY);
	return (cdb_component_dbs == NULL) ? getCdbComponentDatabases() : cdb_component_dbs;
}

#ifdef USE_ASSERT_CHECKING
/**
 * Assert that slicetable is valid. Must be called after ExecInitMotion, which sets up the slice table
 */
void
AssertSliceTableIsValid(SliceTable *st, struct PlannedStmt *pstmt)
{
	if (!st)
	{
		return;
	}

	Assert(st);
	Assert(pstmt);

	Assert(pstmt->nMotionNodes == st->nMotions);
	Assert(pstmt->nInitPlans == st->nInitPlans);

	ListCell *lc = NULL;
	int i = 0;

	int maxIndex = st->nMotions + st->nInitPlans + 1;

	Assert(maxIndex == list_length(st->slices));

	foreach (lc, st->slices)
	{
		Slice *s = (Slice *) lfirst(lc);

		/* The n-th slice entry has sliceIndex of n */
		Assert(s->sliceIndex == i && "slice index incorrect");

		/* The root index of a slice is either 0 or is a slice corresponding to an init plan */
		Assert((s->rootIndex == 0)
				|| (s->rootIndex > st->nMotions && s->rootIndex < maxIndex));

		/* Parent slice index */
		if (s->sliceIndex == s->rootIndex )
		{
			/* Current slice is a root slice. It will have parent index -1.*/
			Assert(s->parentIndex == -1 && "expecting parent index of -1");
		}
		else
		{
			/* All other slices must have a valid parent index */
			Assert(s->parentIndex >= 0 && s->parentIndex < maxIndex && "slice's parent index out of range");
		}

		/* Current slice's children must consider it the parent */
		ListCell *lc1 = NULL;
		foreach (lc1, s->children)
		{
			int childIndex = lfirst_int(lc1);
			Assert(childIndex >= 0 && childIndex < maxIndex && "invalid child slice");
			Slice *sc = (Slice *) list_nth(st->slices, childIndex);
			Assert(sc->parentIndex == s->sliceIndex && "slice's child does not consider it the parent");
		}

		/* Current slice must be in its parent's children list */
		if (s->parentIndex >= 0)
		{
			Slice *sp = (Slice *) list_nth(st->slices, s->parentIndex);

			bool found = false;
			foreach (lc1, sp->children)
			{
				int childIndex = lfirst_int(lc1);
				Assert(childIndex >= 0 && childIndex < maxIndex && "invalid child slice");
				Slice *sc = (Slice *) list_nth(st->slices, childIndex);

				if (sc->sliceIndex == s->sliceIndex)
				{
					found = true;
					break;
				}
			}
			Assert(found && "slice's parent does not consider it a child");
		}
		i++;
	}
}

#endif

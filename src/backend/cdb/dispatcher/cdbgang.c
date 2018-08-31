/*-------------------------------------------------------------------------
 *
 * cdbgang.c
 *	  Query Executor Factory for gangs of QEs
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/dispatcher/cdbgang.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <unistd.h>				/* getpid() */
#include <pthread.h>
#include <limits.h>

#include "libpq-fe.h"
#include "miscadmin.h"			/* MyDatabaseId */
#include "pgstat.h"			/* pgstat_report_sessionid() */
#include "storage/proc.h"		/* MyProc */
#include "storage/ipc.h"
#include "utils/memutils.h"

#include "access/xact.h"
#include "catalog/namespace.h"
#include "commands/variable.h"
#include "nodes/execnodes.h"	/* CdbProcess, Slice, SliceTable */
#include "postmaster/postmaster.h"
#include "tcop/tcopprot.h"
#include "utils/int8.h"
#include "utils/portal.h"
#include "utils/sharedsnapshot.h"
#include "tcop/pquery.h"

#include "libpq-fe.h"
#include "libpq-int.h"
#include "cdb/cdbconn.h"		/* SegmentDatabaseDescriptor */
#include "cdb/cdbfts.h"
#include "cdb/cdbdisp.h"		/* me */
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbgang.h"		/* me */
#include "cdb/cdbgang_thread.h"
#include "cdb/cdbgang_async.h"
#include "cdb/cdbtm.h"			/* discardDtxTransaction() */
#include "cdb/cdbutil.h"		/* CdbComponentDatabaseInfo */
#include "cdb/cdbvars.h"		/* Gp_role, etc. */
#include "cdb/cdbconn.h"		/* Gp_role, etc. */
#include "storage/bfz.h"
#include "libpq/libpq-be.h"
#include "libpq/ip.h"

#include "utils/guc_tables.h"

/*
 * Unique QE identifier, it's set when the QE is started up, it
 * will be use to go through slice table to find which slice
 * this QE should execute.
 */
int			qe_identifier = 0;

/*
 * number of primary segments on this host
 */
int			host_segments = 0;

Gang      *CurrentGangCreating = NULL;

CreateGangFunc pCreateGangFunc = NULL;

static int	largest_gangsize = 0;

static bool NeedResetSession = false;
static Oid	OldTempNamespace = InvalidOid;

static Gang *createGang(GangType type, int gang_id, int size, int content);

static void resetSessionForPrimaryGangLoss(void);

/*
 * Create a reader gang.
 *
 * @type can be GANGTYPE_ENTRYDB_READER, GANGTYPE_SINGLETON_READER or GANGTYPE_PRIMARY_READER.
 */
Gang *
AllocateReaderGang(CdbDispatcherState *ds, GangType type, char *portal_name)
{
	Gang	   *gp = NULL;
	int			size = 0;
	int			content = 0;
	MemoryContext oldContext;

	if (Gp_role != GP_ROLE_DISPATCH)
	{
		elog(FATAL, "dispatch process called with role %d", Gp_role);
	}

	switch (type)
	{
		case GANGTYPE_ENTRYDB_READER:
			content = -1;
			size = 1;
			break;

		case GANGTYPE_SINGLETON_READER:
			content = gp_singleton_segindex;
			size = 1;
			break;

		case GANGTYPE_PRIMARY_READER:
			content = 0;
			size = getgpsegmentCount();
			break;

		default:
			Assert(false);
	}

	ELOG_DISPATCHER_DEBUG("Creating a new reader size %d gang for %s",
						  size, (portal_name ? portal_name : "unnamed portal"));

	oldContext = MemoryContextSwitchTo(DispatcherContext);

	gp = createGang(type, 0, size, content);
	gp->allocated = true;
	ds->allocatedGangs = lcons(gp, ds->allocatedGangs);

	/*
	 * make sure no memory is still allocated for previous portal name that
	 * this gang belonged to
	 */
	if (gp->portal_name)
		pfree(gp->portal_name);

	/* let the gang know which portal it is being assigned to */
	gp->portal_name = (portal_name ? pstrdup(portal_name) : (char *) NULL);

	MemoryContextSwitchTo(oldContext);

	return gp;
}

/*
 * Create a writer gang.
 */
Gang *
AllocateWriterGang(CdbDispatcherState *ds)
{
	MemoryContext oldContext;
	Gang	   *writerGang = NULL;
	int			i = 0;
	int nsegdb = getgpsegmentCount();

	ELOG_DISPATCHER_DEBUG("AllocateWriterGang begin.");

	if (Gp_role != GP_ROLE_DISPATCH)
	{
		elog(FATAL, "dispatch process called with role %d", Gp_role);
	}

	oldContext = MemoryContextSwitchTo(DispatcherContext);

	writerGang = createGang(GANGTYPE_PRIMARY_WRITER, 0, nsegdb, -1);
	writerGang->allocated = true;

	/*
	 * set "whoami" for utility statement. non-utility statement will
	 * overwrite it in function getCdbProcessList.
	 */
	for (i = 0; i < writerGang->size; i++)
		cdbconn_setQEIdentifier(writerGang->db_descriptors[i], -1);

	ELOG_DISPATCHER_DEBUG("AllocateWriterGang end.");

	ds->allocatedGangs = lcons(writerGang, ds->allocatedGangs);

	MemoryContextSwitchTo(oldContext);

	return writerGang;
}

/*
 * Creates a new gang by logging on a session to each segDB involved.
 *
 * call this function in GangContext memory context.
 * elog ERROR or return a non-NULL gang.
 */
static Gang *
createGang(GangType type, int gang_id, int size, int content)
{
	return pCreateGangFunc(type, gang_id, size, content);
}

/*
 * Check the segment failure reason by comparing connection error message.
 */
bool
segment_failure_due_to_recovery(const char *error_message)
{
	char	   *fatal = NULL,
			   *ptr = NULL;
	int			fatal_len = 0;

	if (error_message == NULL)
		return false;

	fatal = _("FATAL");
	fatal_len = strlen(fatal);

	/*
	 * it would be nice if we could check errcode for
	 * ERRCODE_CANNOT_CONNECT_NOW, instead we wind up looking for at the
	 * strings.
	 *
	 * And because if LC_MESSAGES gets set to something which changes the
	 * strings a lot we have to take extreme care with looking at the string.
	 */
	ptr = strstr(error_message, fatal);
	if ((ptr != NULL) && ptr[fatal_len] == ':')
	{
		if (strstr(error_message, _(POSTMASTER_IN_STARTUP_MSG)))
		{
			return true;
		}
		if (strstr(error_message, _(POSTMASTER_IN_RECOVERY_MSG)))
		{
			return true;
		}
		/* We could do retries for "sorry, too many clients already" here too */
	}

	return false;
}

/*
 * Reads the GP catalog tables and build a CdbComponentDatabases structure.
 * It then converts this to a Gang structure and initializes all the non-connection related fields.
 *
 * Call this function in GangContext.
 * Returns a not-null pointer.
 */
Gang *
buildGangDefinition(GangType type, int gang_id, int size, int content)
{
	Gang	   *newGangDefinition = NULL;
	SegmentDatabaseDescriptor *segdbDesc = NULL;

	int			segCount = 0;
	int			i = 0;

	ELOG_DISPATCHER_DEBUG("buildGangDefinition:Starting %d qExec processes for %s gang",
						  size, gangTypeToString(type));

	Assert(CurrentMemoryContext == DispatcherContext);
	Assert(size == 1 || size == getgpsegmentCount());

	/* allocate a gang */
	newGangDefinition = (Gang *) palloc0(sizeof(Gang));
	newGangDefinition->type = type;
	newGangDefinition->size = size;
	newGangDefinition->gang_id = gang_id;
	newGangDefinition->allocated = false;
	newGangDefinition->dispatcherActive = false;
	newGangDefinition->portal_name = NULL;
	newGangDefinition->db_descriptors =
		(SegmentDatabaseDescriptor **) palloc0(size * sizeof(SegmentDatabaseDescriptor*));

	PG_TRY();
	{
		/* initialize db_descriptors */
		switch (type)
		{
			case GANGTYPE_ENTRYDB_READER:
				segdbDesc = cdbcomponent_allocateIdleSegdb(-1, false);
				newGangDefinition->db_descriptors[0] = segdbDesc;
				break;

			case GANGTYPE_SINGLETON_READER:
				segdbDesc = cdbcomponent_allocateIdleSegdb(content, false);
				newGangDefinition->db_descriptors[0] = segdbDesc;
				break;

			case GANGTYPE_PRIMARY_READER:
			case GANGTYPE_PRIMARY_WRITER:

				/*
				 * We loop through the segment_db_info.  Each item has a segindex.
				 * They are sorted by segindex, and there can be > 1
				 * segment_db_info for a given segindex (currently, there can be 1
				 * or 2)
				 */
				for (i = 0; i < getgpsegmentCount(); i++)
				{
					segdbDesc = cdbcomponent_allocateIdleSegdb(i, type == GANGTYPE_PRIMARY_WRITER);

					newGangDefinition->db_descriptors[segCount] = segdbDesc;
					segCount++;
				}

				if (size != segCount)
				{
					elog(ERROR, "Not all primary segment instances are active and connected");
				}
				break;

			default:
				Assert(false);
		}
	}
	PG_CATCH();
	{
		RecycleGang(newGangDefinition, true);
	}
	PG_END_TRY();

	ELOG_DISPATCHER_DEBUG("buildGangDefinition done");
	return newGangDefinition;
}

/*
 * Add one GUC to the option string.
 */
static void
addOneOption(StringInfo string, struct config_generic *guc)
{
	Assert(guc && (guc->flags & GUC_GPDB_ADDOPT));
	switch (guc->vartype)
	{
		case PGC_BOOL:
			{
				struct config_bool *bguc = (struct config_bool *) guc;

				appendStringInfo(string, " -c %s=%s", guc->name, *(bguc->variable) ? "true" : "false");
				break;
			}
		case PGC_INT:
			{
				struct config_int *iguc = (struct config_int *) guc;

				appendStringInfo(string, " -c %s=%d", guc->name, *iguc->variable);
				break;
			}
		case PGC_REAL:
			{
				struct config_real *rguc = (struct config_real *) guc;

				appendStringInfo(string, " -c %s=%f", guc->name, *rguc->variable);
				break;
			}
		case PGC_STRING:
			{
				struct config_string *sguc = (struct config_string *) guc;
				const char *str = *sguc->variable;
				int			i;

				appendStringInfo(string, " -c %s=", guc->name);

				/*
				 * All whitespace characters must be escaped. See
				 * pg_split_opts() in the backend.
				 */
				for (i = 0; str[i] != '\0'; i++)
				{
					if (isspace((unsigned char) str[i]))
						appendStringInfoChar(string, '\\');

					appendStringInfoChar(string, str[i]);
				}
				break;
			}
		case PGC_ENUM:
			{
				struct config_enum *eguc = (struct config_enum *) guc;
				int			value = *eguc->variable;
				const char *str = config_enum_lookup_by_value(eguc, value);
				int			i;

				appendStringInfo(string, " -c %s=", guc->name);

				/*
				 * All whitespace characters must be escaped. See
				 * pg_split_opts() in the backend. (Not sure if an enum value
				 * can have whitespace, but let's be prepared.)
				 */
				for (i = 0; str[i] != '\0'; i++)
				{
					if (isspace((unsigned char) str[i]))
						appendStringInfoChar(string, '\\');

					appendStringInfoChar(string, str[i]);
				}
				break;
			}
		default:
			Insist(false);
	}
}

/*
 * Add GUCs to option string.
 */
char *
makeOptions(void)
{
	struct config_generic **gucs = get_guc_variables();
	int			ngucs = get_num_guc_variables();
	CdbComponentDatabaseInfo *qdinfo = NULL;
	StringInfoData string;
	int			i;

	initStringInfo(&string);

	Assert(Gp_role == GP_ROLE_DISPATCH);

	qdinfo = cdbcomponent_getComponentInfo(-1); 
	appendStringInfo(&string, " -c gp_qd_hostname=%s", qdinfo->hostip);
	appendStringInfo(&string, " -c gp_qd_port=%d", qdinfo->port);

	for (i = 0; i < ngucs; ++i)
	{
		struct config_generic *guc = gucs[i];

		if ((guc->flags & GUC_GPDB_ADDOPT) &&
			(guc->context == PGC_USERSET || procRoleIsSuperuser()))
			addOneOption(&string, guc);
	}

	return string.data;
}

/*
 * build_gpqeid_param
 *
 * Called from the qDisp process to create the "gpqeid" parameter string
 * to be passed to a qExec that is being started.  NB: Can be called in a
 * thread, so mustn't use palloc/elog/ereport/etc.
 */
bool
build_gpqeid_param(char *buf, int bufsz,
				   bool is_writer, int identifier, int hostSegs)
{
	int		len;
#ifdef HAVE_INT64_TIMESTAMP
#define TIMESTAMP_FORMAT INT64_FORMAT
#else
#ifndef _WIN32
#define TIMESTAMP_FORMAT "%.14a"
#else
#define TIMESTAMP_FORMAT "%g"
#endif
#endif

	len = snprintf(buf, bufsz, "%d;" TIMESTAMP_FORMAT ";%s;%d;%d",
				   gp_session_id, PgStartTime,
				   (is_writer ? "true" : "false"), identifier, hostSegs);

	return (len > 0 && len < bufsz);
}

static bool
gpqeid_next_param(char **cpp, char **npp)
{
	*cpp = *npp;
	if (!*cpp)
		return false;

	*npp = strchr(*npp, ';');
	if (*npp)
	{
		**npp = '\0';
		++*npp;
	}
	return true;
}

/*
 * cdbgang_parse_gpqeid_params
 *
 * Called very early in backend initialization, to interpret the "gpqeid"
 * parameter value that a qExec receives from its qDisp.
 *
 * At this point, client authentication has not been done; the backend
 * command line options have not been processed; GUCs have the settings
 * inherited from the postmaster; etc; so don't try to do too much in here.
 */
void
cdbgang_parse_gpqeid_params(struct Port *port __attribute__((unused)),
							const char *gpqeid_value)
{
	char	   *gpqeid = pstrdup(gpqeid_value);
	char	   *cp;
	char	   *np = gpqeid;

	/* The presence of an gpqeid string means this backend is a qExec. */
	SetConfigOption("gp_session_role", "execute", PGC_POSTMASTER, PGC_S_OVERRIDE);

	/* gp_session_id */
	if (gpqeid_next_param(&cp, &np))
		SetConfigOption("gp_session_id", cp, PGC_POSTMASTER, PGC_S_OVERRIDE);

	/* PgStartTime */
	if (gpqeid_next_param(&cp, &np))
	{
#ifdef HAVE_INT64_TIMESTAMP
		if (!scanint8(cp, true, &PgStartTime))
			goto bad;
#else
		PgStartTime = strtod(cp, NULL);
#endif
	}

	/* Gp_is_writer */
	if (gpqeid_next_param(&cp, &np))
		SetConfigOption("gp_is_writer", cp, PGC_POSTMASTER, PGC_S_OVERRIDE);

	/* qe_identifier */
	if (gpqeid_next_param(&cp, &np))
	{
		qe_identifier = (int) strtol(cp, NULL, 10);
	}

	if (gpqeid_next_param(&cp, &np))
	{
		host_segments = (int) strtol(cp, NULL, 10);
	}

	/* Too few items, or too many? */
	if (!cp || np)
		goto bad;

	if (gp_session_id <= 0 || PgStartTime <= 0 || qe_identifier < 0 || host_segments <= 0)
		goto bad;

	pfree(gpqeid);
	return;

bad:
	elog(FATAL, "Segment dispatched with invalid option: 'gpqeid=%s'", gpqeid_value);
}

struct SegmentDatabaseDescriptor *
getSegmentDescriptorFromGang(const Gang *gp, int seg)
{
	int			i = 0;

	if (gp == NULL)
		return NULL;

	for (i = 0; i < gp->size; i++)
	{
		if (gp->db_descriptors[i]->segindex == seg)
			return gp->db_descriptors[i];
	}

	return NULL;
}

static CdbProcess *
makeCdbProcess(SegmentDatabaseDescriptor *segdbDesc)
{
	CdbProcess *process = (CdbProcess *) makeNode(CdbProcess);
	CdbComponentDatabaseInfo *qeinfo = segdbDesc->segment_database_info;

	if (qeinfo == NULL)
	{
		elog(ERROR, "required segment is unavailable");
	}
	else if (qeinfo->hostip == NULL)
	{
		elog(ERROR, "required segment IP is unavailable");
	}

	process->listenerAddr = pstrdup(qeinfo->hostip);

	if (Gp_interconnect_type == INTERCONNECT_TYPE_UDPIFC)
		process->listenerPort = (segdbDesc->motionListener >> 16) & 0x0ffff;
	else if (Gp_interconnect_type == INTERCONNECT_TYPE_TCP)
		process->listenerPort = (segdbDesc->motionListener & 0x0ffff);

	process->pid = segdbDesc->backendPid;
	process->contentid = segdbDesc->segindex;
	return process;
}

/*
 * Create a list of CdbProcess and initialize with Gang information.
 *
 * 1) For primary reader gang and primary writer gang, the elements
 * in this list is order by segment index.
 * 2) For entry DB gang and singleton gang, the list length is 1.
 *
 * @directDispatch: might be null
 */
void
setupCdbProcessList(Slice *slice )
{
	int			i = 0;
	Gang		*gang = slice->primaryGang;
	DirectDispatchInfo *directDispatch = &slice->directDispatch;

	ELOG_DISPATCHER_DEBUG("getCdbProcessList slice%d gangtype=%d gangsize=%d",
						  slice->sliceIndex, gang->type, gang->size);
	Assert(gang);
	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert((gang->type == GANGTYPE_PRIMARY_WRITER && gang->size == getgpsegmentCount()) ||
		   (gang->type == GANGTYPE_PRIMARY_READER && gang->size == getgpsegmentCount()) ||
		   (gang->type == GANGTYPE_ENTRYDB_READER && gang->size == 1) ||
		   (gang->type == GANGTYPE_SINGLETON_READER && gang->size == 1));


	if (directDispatch != NULL && directDispatch->isDirectDispatch)
	{
		/* Currently, direct dispatch is to one segment db. */
		Assert(list_length(directDispatch->contentIds) == 1);

		int			directDispatchContentId = linitial_int(directDispatch->contentIds);
		SegmentDatabaseDescriptor *segdbDesc = gang->db_descriptors[directDispatchContentId];
		CdbProcess *process = makeCdbProcess(segdbDesc);

		cdbconn_setQEIdentifier(segdbDesc, slice->sliceIndex);

		slice->primaryProcesses = lappend(slice->primaryProcesses, process);
		slice->processesMap = bms_add_member(slice->processesMap, segdbDesc->identifier);
	}
	else
	{
		for (i = 0; i < gang->size; i++)
		{
			SegmentDatabaseDescriptor *segdbDesc = gang->db_descriptors[i];
			CdbProcess *process = makeCdbProcess(segdbDesc);

			cdbconn_setQEIdentifier(segdbDesc, slice->sliceIndex);

			slice->primaryProcesses = lappend(slice->primaryProcesses, process);
			slice->processesMap = bms_add_member(slice->processesMap, segdbDesc->identifier);
			ELOG_DISPATCHER_DEBUG("Gang assignment (gang_id %d): slice%d seg%d %s:%d pid=%d",
								  gang->gang_id, slice->sliceIndex, process->contentid,
								  process->listenerAddr, process->listenerPort, process->pid);
		}
	}
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

	qdinfo = cdbcomponent_getComponentInfo(-1);

	Assert(qdinfo->segindex == -1);
	Assert(SEGMENT_IS_ACTIVE_PRIMARY(qdinfo));
	Assert(qdinfo->hostip != NULL);

	proc = makeNode(CdbProcess);

	/*
	 * Set QD listener address to NULL. This will be filled during starting up
	 * outgoing interconnect connection.
	 */
	proc->listenerAddr = NULL;

	if (Gp_interconnect_type == INTERCONNECT_TYPE_UDPIFC)
		proc->listenerPort = (Gp_listener_port >> 16) & 0x0ffff;
	else if (Gp_interconnect_type == INTERCONNECT_TYPE_TCP)
		proc->listenerPort = (Gp_listener_port & 0x0ffff);

	proc->pid = MyProcPid;
	proc->contentid = -1;

	list = lappend(list, proc);
	return list;
}

void
DisconnectAndDestroyAllGangs(bool resetSession)
{
	if (Gp_role == GP_ROLE_UTILITY)
		return;

	ELOG_DISPATCHER_DEBUG("DisconnectAndDestroyAllGangs");

    /* Destroy CurrentGangCreating before GangContext is reset */
    if (CurrentGangCreating != NULL)
    {
        RecycleGang(CurrentGangCreating, true);
        CurrentGangCreating = NULL;
    }

	/* cleanup all out bound dispatcher state */
	CdbResourceOwnerWalker(CurrentResourceOwner, cdbdisp_cleanupDispatcherHandle);
	
	/* destroy cdb_component_dbs, disconnect all connections with QEs */
	cdbcomponent_destroyCdbComponents();

	if (resetSession)
		resetSessionForPrimaryGangLoss();

	ELOG_DISPATCHER_DEBUG("DisconnectAndDestroyAllGangs done");
}

/*
 * Destroy all idle (i.e available) QEs.
 * It is always safe to get rid of the reader QEs.
 *
 * If we are not in a transaction and we do not have a TempNamespace, destroy
 * writer QEs as well.
 *
 * call only from an idle session.
 */
void DisconnectAndDestroyUnusedQEs(void)
{
	if (IsTransactionOrTransactionBlock() || TempNamespaceOidIsValid())
	{
		/*
		 * If we are in a transaction, we can't release the writer gang,
		 * as this will abort the transaction.
		 *
		 * If we have a TempNameSpace, we can't release the writer gang, as this
		 * would drop any temp tables we own.
		 *
		 * Since we are idle, any reader gangs will be available but not allocated.
		 */
		cdbcomponent_cleanupIdleSegdbs(false);
	}
	else
	{
		/*
		 * Get rid of ALL gangs... Readers and primary writer.
		 * After this, we have no resources being consumed on the segDBs at all.
		 *
		 * Our session wasn't destroyed due to an fatal error or FTS action, so
		 * we don't need to do anything special.  Specifically, we DON'T want
		 * to act like we are now in a new session, since that would be confusing
		 * in the log.
		 *
		 */
		cdbcomponent_cleanupIdleSegdbs(true);
	}
}

/*
 * Drop any temporary tables associated with the current session and
 * use a new session id since we have effectively reset the session.
 *
 * Call this procedure outside of a transaction.
 */
void
CheckForResetSession(void)
{
	int			oldSessionId = 0;
	int			newSessionId = 0;
	Oid			dropTempNamespaceOid;

	if (!NeedResetSession)
		return;

	/* Do the session id change early. */

	/* If we have gangs, we can't change our session ID. */
	Assert(!cdbcomponent_segdbsExist());

	oldSessionId = gp_session_id;
	ProcNewMppSessionId(&newSessionId);

	gp_session_id = newSessionId;
	gp_command_count = 0;
	pgstat_report_sessionid(newSessionId);

	/* Update the slotid for our singleton reader. */
	if (SharedLocalSnapshotSlot != NULL)
	{
		LWLockAcquire(SharedLocalSnapshotSlot->slotLock, LW_EXCLUSIVE);
		SharedLocalSnapshotSlot->slotid = gp_session_id;
		LWLockRelease(SharedLocalSnapshotSlot->slotLock);
	}

	elog(LOG, "The previous session was reset because its gang was disconnected (session id = %d). "
		 "The new session id = %d", oldSessionId, newSessionId);

	if (IsTransactionOrTransactionBlock())
	{
		NeedResetSession = false;
		return;
	}

	dropTempNamespaceOid = OldTempNamespace;
	OldTempNamespace = InvalidOid;
	NeedResetSession = false;

	if (dropTempNamespaceOid != InvalidOid)
	{
		PG_TRY();
		{
			DropTempTableNamespaceForResetSession(dropTempNamespaceOid);
		} PG_CATCH();
		{
			/*
			 * But first demote the error to something much less scary.
			 */
			if (!elog_demote(WARNING))
			{
				elog(LOG, "unable to demote error");
				PG_RE_THROW();
			}

			EmitErrorReport();
			FlushErrorState();
		} PG_END_TRY();
	}
}

static void
resetSessionForPrimaryGangLoss(void)
{
	if (ProcCanSetMppSessionId())
	{
		/*
		 * Not too early.
		 */
		NeedResetSession = true;

		/*
		 * Keep this check away from transaction/catalog access, as we are
		 * possibly just after releasing ResourceOwner at the end of Tx. It's
		 * ok to remember uncommitted temporary namespace because
		 * DropTempTableNamespaceForResetSession will simply do nothing if the
		 * namespace is not visible.
		 */
		if (TempNamespaceOidIsValid())
		{
			/*
			 * Here we indicate we don't have a temporary table namespace
			 * anymore so all temporary tables of the previous session will be
			 * inaccessible.  Later, when we can start a new transaction, we
			 * will attempt to actually drop the old session tables to release
			 * the disk space.
			 */
			OldTempNamespace = ResetTempNamespace();

			elog(WARNING,
				 "Any temporary tables for this session have been dropped "
				 "because the gang was disconnected (session id = %d)",
				 gp_session_id);
		}
		else
			OldTempNamespace = InvalidOid;
	}
}

/*
 * Helper functions
 */

int			gp_pthread_create(pthread_t *thread, void *(*start_routine) (void *),
							  void *arg, const char *caller)
{
	int			pthread_err = 0;
	pthread_attr_t t_atts;

	/*
	 * Call some init function. Before any thread is created, we need to init
	 * some static stuff. The main purpose is to guarantee the non-thread safe
	 * stuff are called in main thread, before any child thread get running.
	 * Note these staic data structure should be read only after init.	Thread
	 * creation is a barrier, so there is no need to get lock before we use
	 * these data structures.
	 *
	 * So far, we know we need to do this for getpwuid_r (See MPP-1971, glibc
	 * getpwuid_r is not thread safe).
	 */
#ifndef WIN32
	get_gp_passwdptr();
#endif

	/*
	 * save ourselves some memory: the defaults for thread stack size are
	 * large (1M+)
	 */
	pthread_err = pthread_attr_init(&t_atts);
	if (pthread_err != 0)
	{
		elog(LOG, "%s: pthread_attr_init failed.  Error %d", caller, pthread_err);
		return pthread_err;
	}

	pthread_err = pthread_attr_setstacksize(&t_atts,
											Max(PTHREAD_STACK_MIN, (256 * 1024)));
	if (pthread_err != 0)
	{
		elog(LOG, "%s: pthread_attr_setstacksize failed.  Error %d", caller, pthread_err);
		pthread_attr_destroy(&t_atts);
		return pthread_err;
	}

	pthread_err = pthread_create(thread, &t_atts, start_routine, arg);

	pthread_attr_destroy(&t_atts);

	return pthread_err;
}

const char *
gangTypeToString(GangType type)
{
	const char *ret = "";

	switch (type)
	{
		case GANGTYPE_PRIMARY_WRITER:
			ret = "primary writer";
			break;
		case GANGTYPE_PRIMARY_READER:
			ret = "primary reader";
			break;
		case GANGTYPE_SINGLETON_READER:
			ret = "singleton reader";
			break;
		case GANGTYPE_ENTRYDB_READER:
			ret = "entry DB reader";
			break;
		case GANGTYPE_UNALLOCATED:
			ret = "unallocated";
			break;
		default:
			Assert(false);
	}
	return ret;
}

bool
GangOK(Gang *gp)
{
	int			i;

	if (gp == NULL)
		return false;

	/*
	 * Gang is direct-connect (no agents).
	 */

	for (i = 0; i < gp->size; i++)
	{
		SegmentDatabaseDescriptor *segdbDesc = gp->db_descriptors[i];

		if (cdbconn_isBadConnection(segdbDesc))
			return false;
		if (!FtsIsSegmentUp(segdbDesc->segment_database_info))
			return false;
	}

	return true;
}

int
largestGangsize(void)
{
	return largest_gangsize;
}

void
setLargestGangsize(int size)
{
	if (largest_gangsize < size)
		largest_gangsize = size;
}

void
cdbgang_setAsync(bool async)
{
	if (async)
		pCreateGangFunc = pCreateGangFuncAsync;
	else
		pCreateGangFunc = pCreateGangFuncThreaded;
}

void
RecycleGang(Gang *gp, bool forceDestroy)
{
	int i;

	if (!gp)
		return;

	/*
	 * Loop through the segment_database_descriptors array and, for each
	 * SegmentDatabaseDescriptor: 1) discard the query results (if any), 2)
	 * disconnect the session, and 3) discard any connection error message.
	 */
	for (i = 0; i < gp->size; i++)
	{
		SegmentDatabaseDescriptor *segdbDesc = gp->db_descriptors[i];

		Assert(segdbDesc != NULL);

		cdbcomponent_recycleIdleSegdb(segdbDesc, forceDestroy);
	}
}

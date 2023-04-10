/*-------------------------------------------------------------------------
 *
 * cdbgang.c
 *	  Query Executor Factory for gangs of QEs
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/dispatcher/cdbgang.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "miscadmin.h"			/* MyProcPid */
#include "pgstat.h"			/* pgstat_report_sessionid() */
#include "utils/memutils.h"

#include "access/xact.h"
#include "catalog/namespace.h"
#include "commands/variable.h"
#include "common/ip.h"
#include "nodes/execnodes.h"	/* CdbProcess, Slice, SliceTable */
#include "postmaster/postmaster.h"
#include "tcop/tcopprot.h"
#include "utils/int8.h"
#include "utils/sharedsnapshot.h"
#include "tcop/pquery.h"

#include "libpq-fe.h"
#include "libpq-int.h"
#include "cdb/cdbconn.h"		/* SegmentDatabaseDescriptor */
#include "cdb/cdbfts.h"
#include "cdb/cdbdisp.h"		/* me */
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbgang.h"		/* me */
#include "cdb/cdbgang_async.h"
#include "cdb/cdbutil.h"		/* CdbComponentDatabaseInfo */
#include "cdb/cdbvars.h"		/* Gp_role, etc. */
#include "cdb/cdbconn.h"		/* cdbconn_* */
#include "libpq/libpq-be.h"

#include "utils/guc_tables.h"

#include "funcapi.h"
#include "utils/builtins.h"

/*
 * All QEs are managed by cdb_component_dbs in QD, QD assigned
 * a unique identifier for each QE, when a QE is created, this
 * identifier is passed along with gpqeid params, see
 * cdbgang_parse_gpqeid_params()
 *
 * qe_identifier is use to go through slice table and find which slice
 * this QE should execute.
 */
int			qe_identifier = 0;

/*
 * Number of primary segments on this host.
 * Note: This is only set on the segments and not on the coordinator. It is
 * used primarily by resource groups.
 */
int			host_primary_segment_count = 0;

/*
 * size of hash table of interconnect connections
 * equals to 2 * (the number of total segments)
 */
int			ic_htab_size = 0;

Gang      *CurrentGangCreating = NULL;

CreateGangFunc pCreateGangFunc = cdbgang_createGang_async;

static bool NeedResetSession = false;
static Oid	OldTempNamespace = InvalidOid;

/* WaitEventSet for dispatch */
WaitEventSet *DispWaitSet = NULL;

/*
 * cdbgang_createGang:
 *
 * Creates a new gang by logging on a session to each segDB involved.
 *
 * call this function in GangContext memory context.
 * elog ERROR or return a non-NULL gang.
 */
Gang *
cdbgang_createGang(List *segments, SegmentType segmentType)
{
	Assert(pCreateGangFunc);

	return pCreateGangFunc(segments, segmentType);
}

/*
 * Creates a new gang by logging on a session to each segDB involved.
 *
 * elog ERROR or return a non-NULL gang.
 */
Gang *
AllocateGang(CdbDispatcherState *ds, GangType type, List *segments)
{
	MemoryContext	oldContext;
	SegmentType 	segmentType;
	Gang			*newGang = NULL;
	int				i;

	ELOG_DISPATCHER_DEBUG("AllocateGang begin.");

	if (Gp_role != GP_ROLE_DISPATCH)
	{
		elog(FATAL, "dispatch process called with role %d", Gp_role);
	}

	if (segments == NIL)
		return NULL;

	Assert(DispatcherContext);
	oldContext = MemoryContextSwitchTo(DispatcherContext);

	if (type == GANGTYPE_PRIMARY_WRITER)
		segmentType = SEGMENTTYPE_EXPLICT_WRITER;
	/* for extended query like cursor, must specify a reader */
	else if (ds->isExtendedQuery)
		segmentType = SEGMENTTYPE_EXPLICT_READER;
	else
		segmentType = SEGMENTTYPE_ANY;

	newGang = cdbgang_createGang(segments, segmentType);
	newGang->allocated = true;
	newGang->type = type;

	/*
	 * Push to the head of the allocated list, later in
	 * cdbdisp_destroyDispatcherState() we should recycle them from the head to
	 * restore the original order of the idle gangs.
	 */
	ds->allocatedGangs = lcons(newGang, ds->allocatedGangs);
	ds->largestGangSize = Max(ds->largestGangSize, newGang->size);

	ELOG_DISPATCHER_DEBUG("AllocateGang end.");

	if (type == GANGTYPE_PRIMARY_WRITER)
	{
		/*
		 * set "whoami" for utility statement. non-utility statement will
		 * overwrite it in function getCdbProcessList.
		 */
		for (i = 0; i < newGang->size; i++)
			cdbconn_setQEIdentifier(newGang->db_descriptors[i], -1);
	}

	MemoryContextSwitchTo(oldContext);

	return newGang;
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
		if (strstr(error_message, _(POSTMASTER_IN_RESET_MSG)))
		{
			return true;
		}
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

/* Check if the segment failure is due to missing writer process on QE node. */
bool
segment_failure_due_to_missing_writer(const char *error_message)
{
	char	   *fatal = NULL,
			   *ptr = NULL;
	int			fatal_len = 0;

	if (error_message == NULL)
		return false;

	fatal = _("FATAL");
	fatal_len = strlen(fatal);

	ptr = strstr(error_message, fatal);
	if ((ptr != NULL) && ptr[fatal_len] == ':' &&
		strstr(error_message, _(WRITER_IS_MISSING_MSG)))
		return true;

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
buildGangDefinition(List *segments, SegmentType segmentType)
{
	Gang *newGangDefinition = NULL;
	ListCell *lc;
	volatile int i = 0;
	int	size;
	int contentId;

	size = list_length(segments);

	ELOG_DISPATCHER_DEBUG("buildGangDefinition:Starting %d qExec processes for gang", size);

	Assert(CurrentMemoryContext == DispatcherContext);

	/* allocate a gang */
	newGangDefinition = (Gang *) palloc0(sizeof(Gang));
	newGangDefinition->type = GANGTYPE_UNALLOCATED;
	newGangDefinition->size = size;
	newGangDefinition->allocated = false;
	newGangDefinition->db_descriptors =
		(SegmentDatabaseDescriptor **) palloc0(size * sizeof(SegmentDatabaseDescriptor*));

	PG_TRY();
	{
		/* initialize db_descriptors */
		foreach_with_count (lc, segments , i)
		{
			contentId = lfirst_int(lc);
			newGangDefinition->db_descriptors[i] =
						cdbcomponent_allocateIdleQE(contentId, segmentType);
		}
	}
	PG_CATCH();
	{
		newGangDefinition->size = i;
		RecycleGang(newGangDefinition, true /* destroy */);
		PG_RE_THROW();
	}
	PG_END_TRY();

	ELOG_DISPATCHER_DEBUG("buildGangDefinition done");
	return newGangDefinition;
}

/*
 * Add one GUC to the option string.
 */
static void
addOneOption(StringInfo option, StringInfo diff, struct config_generic *guc)
{
	Assert(guc && (guc->flags & GUC_GPDB_NEED_SYNC));
	switch (guc->vartype)
	{
		case PGC_BOOL:
			{
				struct config_bool *bguc = (struct config_bool *) guc;

				appendStringInfo(option, " -c %s=%s", guc->name, (bguc->reset_val) ? "true" : "false");
				if (bguc->reset_val != *bguc->variable)
					appendStringInfo(diff, " %s=%s", guc->name, *(bguc->variable) ? "true" : "false");
				break;
			}
		case PGC_INT:
			{
				struct config_int *iguc = (struct config_int *) guc;

				appendStringInfo(option, " -c %s=%d", guc->name, iguc->reset_val);
				if (iguc->reset_val != *iguc->variable)
					appendStringInfo(diff, " %s=%d", guc->name, *iguc->variable);
				break;
			}
		case PGC_REAL:
			{
				struct config_real *rguc = (struct config_real *) guc;

				appendStringInfo(option, " -c %s=%f", guc->name, rguc->reset_val);
				if (rguc->reset_val != *rguc->variable)
					appendStringInfo(diff, " %s=%f", guc->name, *rguc->variable);
				break;
			}
		case PGC_STRING:
			{
				struct config_string *sguc = (struct config_string *) guc;
				const char *str = sguc->reset_val;
				int			i;

				appendStringInfo(option, " -c %s=", guc->name);

				/*
				 * All whitespace characters must be escaped. See
				 * pg_split_opts() in the backend.
				 */
				for (i = 0; str[i] != '\0'; i++)
				{
					if (isspace((unsigned char) str[i]))
						appendStringInfoChar(option, '\\');

					appendStringInfoChar(option, str[i]);
				}
				if (strcmp(str, *sguc->variable) != 0)
				{
					const char *p = *sguc->variable;
					appendStringInfo(diff, " %s=", guc->name);
					for (i = 0; p[i] != '\0'; i++)
					{
						if (isspace((unsigned char) p[i]))
							appendStringInfoChar(diff, '\\');

						appendStringInfoChar(diff, p[i]);
					}
				}
				break;
			}
		case PGC_ENUM:
			{
				struct config_enum *eguc = (struct config_enum *) guc;
				int			value = eguc->reset_val;
				const char *str = config_enum_lookup_by_value(eguc, value);
				int			i;

				appendStringInfo(option, " -c %s=", guc->name);

				/*
				 * All whitespace characters must be escaped. See
				 * pg_split_opts() in the backend. (Not sure if an enum value
				 * can have whitespace, but let's be prepared.)
				 */
				for (i = 0; str[i] != '\0'; i++)
				{
					if (isspace((unsigned char) str[i]))
						appendStringInfoChar(option, '\\');

					appendStringInfoChar(option, str[i]);
				}
				if (value != *eguc->variable)
				{
					const char *p = config_enum_lookup_by_value(eguc, *eguc->variable);
					appendStringInfo(diff, " %s=", guc->name);
					for (i = 0; p[i] != '\0'; i++)
					{
						if (isspace((unsigned char) p[i]))
							appendStringInfoChar(diff, '\\');

						appendStringInfoChar(diff, p[i]);
					}
				}
				break;
			}
	}
}

/*
 * Add GUCs to option/diff_options string.
 *
 * `options` is a list of reset_val of the GUCs, not the GUC's current value.
 * `diff_options` is a list of the GUCs' current value. If the GUC is unchanged,
 * `diff_options` will omit it.
 *
 * In process_startup_options(), `options` is used to set the GUCs with
 * PGC_S_CLIENT as its guc source. Then, `diff_options` is used to set the GUCs
 * with PGC_S_SESSION as its guc source.
 *
 * With PGC_S_CLIENT, SetConfigOption() will set the GUC's reset_val
 * when processing `options`, so the reset_val of the involved GUCs on all QD
 * and QEs are the same.
 *
 * After applying `diff_options`, the GUCs' current value is set to the same
 * value as the QD and the reset_val of the GUC will not change.
 *
 * At last, both the reset_val and current value of the GUC are consistent,
 * even after RESET.
 *
 * See addOneOption() and process_startup_options() for more details.
 */
void
makeOptions(char **options, char **diff_options)
{
	struct config_generic **gucs = get_guc_variables();
	int			ngucs = get_num_guc_variables();
	CdbComponentDatabaseInfo *qdinfo = NULL;
	StringInfoData optionsStr;
	StringInfoData diffStr;
	int			i;

	initStringInfo(&optionsStr);
	initStringInfo(&diffStr);

	Assert(Gp_role == GP_ROLE_DISPATCH);

	qdinfo = cdbcomponent_getComponentInfo(MASTER_CONTENT_ID); 
	appendStringInfo(&optionsStr, " -c gp_qd_hostname=%s", qdinfo->config->hostip);
	appendStringInfo(&optionsStr, " -c gp_qd_port=%d", qdinfo->config->port);

	for (i = 0; i < ngucs; ++i)
	{
		struct config_generic *guc = gucs[i];

		if ((guc->flags & GUC_GPDB_NEED_SYNC) &&
			(guc->context == PGC_USERSET ||
			 guc->context == PGC_BACKEND ||
			 IsAuthenticatedUserSuperUser()))
			addOneOption(&optionsStr, &diffStr, guc);
	}

	*options = optionsStr.data;
	*diff_options = diffStr.data;
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
				   bool is_writer, int identifier, int hostSegs, int icHtabSize)
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

	len = snprintf(buf, bufsz, "%d;" TIMESTAMP_FORMAT ";%s;%d;%d;%d",
				   gp_session_id, PgStartTime,
				   (is_writer ? "true" : "false"), identifier, hostSegs, icHtabSize);

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
cdbgang_parse_gpqeid_params(struct Port *port pg_attribute_unused(),
							const char *gpqeid_value)
{
	char	   *gpqeid = pstrdup(gpqeid_value);
	char	   *cp;
	char	   *np = gpqeid;

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
		host_primary_segment_count = (int) strtol(cp, NULL, 10);
	}

	if (gpqeid_next_param(&cp, &np))
	{
		ic_htab_size = (int) strtol(cp, NULL, 10);
	}

	/* Too few items, or too many? */
	if (!cp || np)
		goto bad;

	if (gp_session_id <= 0 || PgStartTime <= 0 || qe_identifier < 0 ||
		host_primary_segment_count <= 0 || ic_htab_size <= 0)
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
	else if (qeinfo->config->hostip == NULL)
	{
		elog(ERROR, "required segment IP is unavailable");
	}

	process->listenerAddr = pstrdup(qeinfo->config->hostip);

	if (Gp_interconnect_type == INTERCONNECT_TYPE_UDPIFC)
		process->listenerPort = (segdbDesc->motionListener >> 16) & 0x0ffff;
	else if (Gp_interconnect_type == INTERCONNECT_TYPE_TCP ||
			 Gp_interconnect_type == INTERCONNECT_TYPE_PROXY)
		process->listenerPort = (segdbDesc->motionListener & 0x0ffff);

	process->pid = segdbDesc->backendPid;
	process->contentid = segdbDesc->segindex;
	process->dbid = qeinfo->config->dbid;
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
setupCdbProcessList(ExecSlice *slice)
{
	int			i = 0;
	Gang		*gang = slice->primaryGang;

	ELOG_DISPATCHER_DEBUG("getCdbProcessList slice%d gangtype=%d gangsize=%d",
						  slice->sliceIndex, gang->type, gang->size);
	Assert(gang);
	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(gang->type == GANGTYPE_PRIMARY_WRITER ||
		   gang->type == GANGTYPE_PRIMARY_READER ||
		   (gang->type == GANGTYPE_ENTRYDB_READER && gang->size == 1) ||
		   (gang->type == GANGTYPE_SINGLETON_READER && gang->size == 1));


	for (i = 0; i < gang->size; i++)
	{
		SegmentDatabaseDescriptor *segdbDesc = gang->db_descriptors[i];
		CdbProcess *process = makeCdbProcess(segdbDesc);

		cdbconn_setQEIdentifier(segdbDesc, slice->sliceIndex);

		slice->primaryProcesses = lappend(slice->primaryProcesses, process);
		slice->processesMap = bms_add_member(slice->processesMap, segdbDesc->identifier);

		ELOG_DISPATCHER_DEBUG("Gang assignment: slice%d seg%d %s:%d pid=%d",
							  slice->sliceIndex, process->contentid,
							  process->listenerAddr, process->listenerPort,
							  process->pid);
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

	qdinfo = cdbcomponent_getComponentInfo(MASTER_CONTENT_ID);

	Assert(qdinfo->config->segindex == -1);
	Assert(SEGMENT_IS_ACTIVE_PRIMARY(qdinfo));
	Assert(qdinfo->config->hostip != NULL);

	proc = makeNode(CdbProcess);

	/*
	 * Set QD listener address to the ADDRESS of the master, so the motions that connect to
	 * the master knows what the interconnect address of the peer is.
	 */
	proc->listenerAddr = pstrdup(qdinfo->config->hostip);

	if (Gp_interconnect_type == INTERCONNECT_TYPE_UDPIFC)
		proc->listenerPort = (Gp_listener_port >> 16) & 0x0ffff;
	else if (Gp_interconnect_type == INTERCONNECT_TYPE_TCP ||
			 Gp_interconnect_type == INTERCONNECT_TYPE_PROXY)
		proc->listenerPort = (Gp_listener_port & 0x0ffff);

	proc->pid = MyProcPid;
	proc->contentid = -1;
	proc->dbid = qdinfo->config->dbid;

	list = lappend(list, proc);
	return list;
}

/*
 * This function should not be used in the context of named portals
 * as it destroys the CdbComponentsContext, which is accessed later
 * during named portal cleanup.
 */
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
 * call only from an idle session.
 */
void DisconnectAndDestroyUnusedQEs(void)
{
	/*
	 * Only release reader gangs, never writer gang. This helps to avoid the
	 * shared snapshot collision error on next gang creation from hitting if
	 * QE processes are slow to exit due to this cleanup.
	 *
	 * If we are in a transaction, we can't release the writer gang also, as
	 * this will abort the transaction.
	 *
	 * If we have a TempNameSpace, we can't release the writer gang also, as
	 * this would drop any temp tables we own.
	 *
	 * Since we are idle, any reader gangs will be available but not
	 * allocated.
	 */
	cdbcomponent_cleanupIdleQEs(false);
}

/*
 * Drop any temporary tables associated with the current session and
 * use a new session id since we have effectively reset the session.
 */
void
GpDropTempTables(void)
{
	int			oldSessionId = 0;
	int			newSessionId = 0;
	Oid			dropTempNamespaceOid;

	/* No need to reset session or drop temp tables */
	if (!NeedResetSession && OldTempNamespace == InvalidOid)
		return;

	/* Do the session id change early. */
	if (NeedResetSession)
	{
		/* If we have gangs, we can't change our session ID. */
		Assert(!cdbcomponent_qesExist());

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
	}

	/*
	 * When it's in transaction block, need to bump the session id, e.g. retry COMMIT PREPARED,
	 * but defer drop temp table to the main loop in PostgresMain().
	 */
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

void
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

void
RecycleGang(Gang *gp, bool forceDestroy)
{
	int i;

	if (!gp)
		return;
	/*
	 *
	 * Callers of RecycleGang should not throw ERRORs by design. This is
	 * because RecycleGang is not re-entrant: For example, an ERROR could be
	 * thrown whilst the gang's segdbDesc is already freed. This would cause
	 * RecycleGang to be called again during abort processing, giving rise to
	 * potential double freeing of the gang's segdbDesc.
	 *
	 * Thus, we hold off interrupts until the gang is fully cleaned here to prevent
	 * throwing an ERROR here.
	 *
	 * details See github issue: https://github.com/greenplum-db/gpdb/issues/13393
	 */
	HOLD_INTERRUPTS();
	/*
	 * Loop through the segment_database_descriptors array and, for each
	 * SegmentDatabaseDescriptor: 1) discard the query results (if any), 2)
	 * disconnect the session, and 3) discard any connection error message.
	 */
#ifdef FAULT_INJECTOR
	/*
	 * select * from gp_segment_configuration a, t13393,
	 * gp_segment_configuration b where a.dbid = t13393.tc1 limit 0;
	 *
	 * above sql has 3 gangs, the first and second gangtype is ENTRYDB_READER
	 * and the third gang is PRIMARY_READER, the second gang will be destroyed.
	 * inject an interrupt fault during RecycleGang PRIMARY_READER gang.
	 */
	if (gp->size >= 3)
	{
		SIMPLE_FAULT_INJECTOR("cdbcomponent_recycle_idle_qe_error");
		CHECK_FOR_INTERRUPTS();
	}
#endif
	for (i = 0; i < gp->size; i++)
	{
		SegmentDatabaseDescriptor *segdbDesc = gp->db_descriptors[i];

		Assert(segdbDesc != NULL);

		cdbcomponent_recycleIdleQE(segdbDesc, forceDestroy);
	}
	RESUME_INTERRUPTS();
}

void
ResetAllGangs(void)
{
	DisconnectAndDestroyAllGangs(true);
	GpDropTempTables();
}

/*
 * Used by gp_backend_info() to find a single character that represents a
 * backend type.
 */
static char
backend_type(SegmentDatabaseDescriptor *segdb)
{
	if (segdb->identifier == -1)
	{
		/* QD backend */
		return 'Q';
	}
	if (segdb->segindex == -1)
	{
		/* Entry singleton reader. */
		return 'R';
	}

	return (segdb->isWriter ? 'w' : 'r');
}

/*
 * qsort comparator for SegmentDatabaseDescriptors. Sorts by descriptor ID.
 */
static int
compare_segdb_id(const void *v1, const void *v2)
{
	SegmentDatabaseDescriptor *d1 = (SegmentDatabaseDescriptor *) lfirst(*(ListCell **) v1);
	SegmentDatabaseDescriptor *d2 = (SegmentDatabaseDescriptor *) lfirst(*(ListCell **) v2);

	return d1->identifier - d2->identifier;
}

/*
 * Returns a list of rows, each corresponding to a connected segment backend and
 * containing information on the role and definition of that backend (e.g. host,
 * port, PID).
 *
 * SELECT * from gp_backend_info();
 */
Datum
gp_backend_info(PG_FUNCTION_ARGS)
{
	if (Gp_role != GP_ROLE_DISPATCH)
		ereport(ERROR, (errcode(ERRCODE_GP_COMMAND_ERROR),
			errmsg("gp_backend_info() could only be called on QD")));

	/* Our struct for funcctx->user_fctx. */
	struct func_ctx
	{
		List	   *segdbs;		/* the SegmentDatabaseDescriptor entries we will output */
		ListCell   *curpos;		/* pointer to our current position in .segdbs */
	};

	FuncCallContext *funcctx;
	struct func_ctx *user_fctx;

	/* Number of attributes we'll return per row. Must match the catalog. */
#define BACKENDINFO_NATTR    6

	if (SRF_IS_FIRSTCALL())
	{
		/* Standard first-call setup. */
		MemoryContext         oldcontext;
		TupleDesc             tupdesc;
		CdbComponentDatabases *cdbs;
		int                   i;

		funcctx    = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
		funcctx->user_fctx = user_fctx = palloc0(sizeof(*user_fctx));

		/* Construct the list of all known segment DB descriptors. */
		cdbs = cdbcomponent_getCdbComponents();

		for (i = 0; i < cdbs->total_entry_dbs; ++i)
		{
			CdbComponentDatabaseInfo *cdbinfo = &cdbs->entry_db_info[i];

			user_fctx->segdbs =
				list_concat_unique_ptr(user_fctx->segdbs, cdbinfo->activelist);
			user_fctx->segdbs =
				list_concat_unique_ptr(user_fctx->segdbs, cdbinfo->freelist);
		}

		for (i = 0; i < cdbs->total_segment_dbs; ++i)
		{
			CdbComponentDatabaseInfo *cdbinfo = &cdbs->segment_db_info[i];

			user_fctx->segdbs =
				list_concat_unique_ptr(user_fctx->segdbs, cdbinfo->activelist);
			user_fctx->segdbs =
				list_concat_unique_ptr(user_fctx->segdbs, cdbinfo->freelist);
		}
		/* Fake a segment descriptor to represent the current QD backend */
		SegmentDatabaseDescriptor *qddesc = palloc0(sizeof(SegmentDatabaseDescriptor));
		qddesc->segment_database_info = cdbcomponent_getComponentInfo(MASTER_CONTENT_ID);
		qddesc->segindex = -1;
		qddesc->conn = NULL;
		qddesc->motionListener = 0;
		qddesc->backendPid = MyProcPid;
		qddesc->whoami = NULL;
		qddesc->isWriter = false;
		qddesc->identifier = -1;

		user_fctx->segdbs = lcons(qddesc, user_fctx->segdbs);
		/*
		 * For a slightly better default user experience, sort by descriptor ID.
		 * Users may of course specify their own ORDER BY if they don't like it.
		 */
		user_fctx->segdbs = list_qsort(user_fctx->segdbs, compare_segdb_id);
		user_fctx->curpos = list_head(user_fctx->segdbs);

		/* Create a descriptor for the records we'll be returning. */
		tupdesc = CreateTemplateTupleDesc(BACKENDINFO_NATTR);
		TupleDescInitEntry(tupdesc, 1, "id", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, 2, "type", CHAROID, -1, 0);
		TupleDescInitEntry(tupdesc, 3, "content", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, 4, "host", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, 5, "port", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, 6, "pid", INT4OID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/* Tell the caller how many rows we'll return. */
		funcctx->max_calls = list_length(user_fctx->segdbs);

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();

	/* Construct and return a row for every entry. */
	if (funcctx->call_cntr < funcctx->max_calls)
	{
		Datum                     values[BACKENDINFO_NATTR] = {0};
		bool nulls[BACKENDINFO_NATTR]                       = {0};
		HeapTuple                 tuple;
		SegmentDatabaseDescriptor *dbdesc;
		CdbComponentDatabaseInfo  *dbinfo;

		user_fctx = funcctx->user_fctx;

		/* Get the next descriptor. */
		dbdesc = lfirst(user_fctx->curpos);
		user_fctx->curpos = lnext(user_fctx->curpos);

		/* Fill in the row attributes. */
		dbinfo = dbdesc->segment_database_info;

		values[0] = Int32GetDatum(dbdesc->identifier);			/* id */
		values[1] = CharGetDatum(backend_type(dbdesc));	/* type */
		values[2] = Int32GetDatum(dbdesc->segindex);			/* content */

		if (dbinfo->config->hostname)								/* host */
			values[3] = CStringGetTextDatum(dbinfo->config->hostname);
		else
			nulls[3] = true;

		values[4] = Int32GetDatum(dbinfo->config->port);          /* port */
		values[5] = Int32GetDatum(dbdesc->backendPid);            /* pid */

		/* Form the new tuple using our attributes and return it. */
		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);

		SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
	}
	else
	{
		/* Clean up. */
		user_fctx = funcctx->user_fctx;
		if (user_fctx)
		{
			list_free(user_fctx->segdbs);
			pfree(user_fctx);

			funcctx->user_fctx = NULL;
		}

		SRF_RETURN_DONE(funcctx);
	}
#undef BACKENDINFO_NATTR
}

/*
 * Print the time of create a gang.
 * if all segDescs of the gang are cached, we regard the gang as reused.
 * else we print the shortest time and the longest time of estabishing connection to the segDesc.
 */
void
printCreateGangTime(int sliceId, Gang *gang)
{
	double	shortestTime = -1, longestTime = -1;
	int		shortestSegIndex = -1, longestSegIndex = -1;
	int		size = gang->size;
	SegmentDatabaseDescriptor *segdbDesc;
	for (int i = 0; i < size; i++)
	{
		segdbDesc = gang->db_descriptors[i];
		/* the connection of segdbDesc is not cached */
		if (segdbDesc->establishConnTime != -1)
		{
			if (longestTime == -1 || segdbDesc->establishConnTime > longestTime)
			{
				longestTime = segdbDesc->establishConnTime;
				longestSegIndex = segdbDesc->segindex;
			}
			if (shortestTime == -1 || segdbDesc->establishConnTime < shortestTime)
			{
				shortestTime = segdbDesc->establishConnTime;
				shortestSegIndex = segdbDesc->segindex;
			}
		}
	}

	/* All the segDescs are cached, and we regard this gang as reused gang. */
	if (longestTime == -1)
	{
		if (sliceId == -1)
		{
			elog(INFO, "(Gang) is reused");
		}
		else
		{
			elog(INFO, "(Slice%d) is reused", sliceId);
		}

	}
	else
	{
		if (sliceId == -1)
		{
			elog(INFO, "The shortest establish conn time: %.2f ms, segindex: %d,\n"
				"       The longest  establish conn time: %.2f ms, segindex: %d",
				shortestTime, shortestSegIndex, longestTime, longestSegIndex);
			}
		else
		{
			elog(INFO, "(Slice%d) The shortest establish conn time: %.2f ms, segindex: %d,\n"
				 "                The longest  establish conn time: %.2f ms, segindex: %d",
				sliceId, shortestTime, shortestSegIndex, longestTime, longestSegIndex);
		}
	}
}

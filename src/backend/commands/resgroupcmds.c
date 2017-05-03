/*-------------------------------------------------------------------------
 *
 * resgroupcmds.c
 *	  Commands for manipulating resource group.
 *
 * Copyright (c) 2006-2017, Greenplum inc.
 *
 * IDENTIFICATION
 *    src/backend/commands/resgroupcmds.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "funcapi.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/xact.h"
#include "catalog/heap.h"
#include "catalog/oid_dispatch.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_resgroup.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbvars.h"
#include "commands/comment.h"
#include "commands/defrem.h"
#include "commands/resgroupcmds.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/datetime.h"
#include "utils/fmgroids.h"
#include "utils/resgroup.h"
#include "utils/resource_manager.h"
#include "utils/syscache.h"

#define RESGROUP_DEFAULT_CONCURRENCY (20)
#define RESGROUP_DEFAULT_REDZONE_LIMIT (0.8)


/*
 * Internal struct to store the group settings.
 */
typedef struct ResourceGroupOptions
{
	int concurrency;
	float cpuRateLimit;
	float memoryLimit;
	float redzoneLimit;
} ResourceGroupOptions;

static float text2Float(const text *text, const char *prop);
static int getResgroupOptionType(const char* defname);
static void parseStmtOptions(CreateResourceGroupStmt *stmt, ResourceGroupOptions *options);
static void validateCapabilities(Relation rel, Oid groupid, ResourceGroupOptions *options);
static void getResgroupCapabilityEntry(int groupId, int type, char **value, char **proposed);
static void insertResgroupCapabilityEntry(Relation rel, Oid groupid, uint16 type, char *value);
static void updateResgroupCapabilityEntry(Oid groupid, uint16 type, char *value, char *proposed);
static void insertResgroupCapabilities(Oid groupid, ResourceGroupOptions *options);
static void deleteResgroupCapabilities(Oid groupid);
static void createResGroupAbortCallback(ResourceReleasePhase phase, bool isCommit, bool isTopLevel, void *arg);
static void dropResGroupAbortCallback(ResourceReleasePhase phase, bool isCommit, bool isTopLevel, void *arg);
static void alterResGroupCommitCallback(ResourceReleasePhase phase, bool isCommit, bool isTopLevel, void *arg);

/*
 * CREATE RESOURCE GROUP
 */
void
CreateResourceGroup(CreateResourceGroupStmt *stmt)
{
	Relation	pg_resgroup_rel;
	TupleDesc	pg_resgroup_dsc;
	ScanKeyData scankey;
	SysScanDesc sscan;
	HeapTuple	tuple;
	Oid			groupid;
	Datum		new_record[Natts_pg_resgroup];
	bool		new_record_nulls[Natts_pg_resgroup];
	ResourceGroupOptions options;
	int			nResGroups;

	/* Permission check - only superuser can create groups. */
	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("must be superuser to create resource groups")));

	/*
	 * Check for an illegal name ('none' is used to signify no group in ALTER
	 * ROLE).
	 */
	if (strcmp(stmt->name, "none") == 0)
		ereport(ERROR,
				(errcode(ERRCODE_RESERVED_NAME),
				 errmsg("resource group name \"none\" is reserved")));

	MemSet(&options, 0, sizeof(options));
	parseStmtOptions(stmt, &options);

	/*
	 * Grant ExclusiveLock to serialize concurrent 'CREATE RESOURCE GROUP'
	 */
	pg_resgroup_rel = heap_open(ResGroupRelationId, ExclusiveLock);

	/* Check if max_resource_group limit is reached */
	sscan = systable_beginscan(pg_resgroup_rel, InvalidOid, false,
							   SnapshotNow, 0, NULL);
	nResGroups = 0;
	while (systable_getnext(sscan) != NULL)
		nResGroups++;
	systable_endscan(sscan);

	if (nResGroups >= MaxResourceGroups)
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
				 errmsg("insufficient resource groups available"),
				 errhint("Increase max_resource_groups")));

	/*
	 * Check the pg_resgroup relation to be certain the group doesn't already
	 * exist.
	 */
	ScanKeyInit(&scankey,
				Anum_pg_resgroup_rsgname,
				BTEqualStrategyNumber, F_NAMEEQ,
				CStringGetDatum(stmt->name));

	sscan = systable_beginscan(pg_resgroup_rel, ResGroupRsgnameIndexId, true,
							   SnapshotNow, 1, &scankey);

	if (HeapTupleIsValid(systable_getnext(sscan)))
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_OBJECT),
				 errmsg("resource group \"%s\" already exists",
						stmt->name)));

	systable_endscan(sscan);

	/*
	 * Build a tuple to insert
	 */
	MemSet(new_record, 0, sizeof(new_record));
	MemSet(new_record_nulls, false, sizeof(new_record_nulls));

	new_record[Anum_pg_resgroup_rsgname - 1] =
		DirectFunctionCall1(namein, CStringGetDatum(stmt->name));

	new_record[Anum_pg_resgroup_parent - 1] = Int64GetDatum(0);

	pg_resgroup_dsc = RelationGetDescr(pg_resgroup_rel);
	tuple = heap_form_tuple(pg_resgroup_dsc, new_record, new_record_nulls);

	/*
	 * Insert new record in the pg_resgroup table
	 */
	groupid = simple_heap_insert(pg_resgroup_rel, tuple);
	CatalogUpdateIndexes(pg_resgroup_rel, tuple);

	/* process the WITH (...) list items */
	insertResgroupCapabilities(groupid, &options);

	/* Dispatch the statement to segments */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR|
									DF_WITH_SNAPSHOT|
									DF_NEED_TWO_PHASE,
									GetAssignedOidsForDispatch(),
									NULL);
		MetaTrackAddObject(ResGroupRelationId,
						   groupid,
						   GetUserId(), /* not ownerid */
						   "CREATE", "RESOURCE GROUP");
	}

	heap_close(pg_resgroup_rel, NoLock);

	/* Add this group into shared memory */
	if (IsResGroupEnabled())
	{
		Oid			*callbackArg;

		AllocResGroupEntry(groupid);

		/* Argument of callback function should be allocated in heap region */
		callbackArg = (Oid *)MemoryContextAlloc(TopMemoryContext, sizeof(Oid));
		*callbackArg = groupid;
		RegisterResourceReleaseCallback(createResGroupAbortCallback, (void *)callbackArg);
	}
	else if (Gp_role == GP_ROLE_DISPATCH)
		ereport(WARNING,
				(errmsg("resource group is disabled"),
				 errhint("To enable set resource_scheduler=on and gp_resource_manager=group")));
}

/*
 * DROP RESOURCE GROUP
 */
void
DropResourceGroup(DropResourceGroupStmt *stmt)
{
	Relation	 pg_resgroup_rel;
	Relation	 authIdRel;
	HeapTuple	 tuple;
	ScanKeyData	 scankey;
	SysScanDesc	 sscan;
	ScanKeyData	 authid_scankey;
	SysScanDesc	 authid_scan;
	Oid			 groupid;
	Oid			*callbackArg;

	/* Permission check - only superuser can drop resource groups. */
	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("must be superuser to drop resource groups")));

	/*
	 * Check the pg_resgroup relation to be certain the resource group already
	 * exists.
	 */
	pg_resgroup_rel = heap_open(ResGroupRelationId, RowExclusiveLock);

	ScanKeyInit(&scankey,
				Anum_pg_resgroup_rsgname,
				BTEqualStrategyNumber, F_NAMEEQ,
				CStringGetDatum(stmt->name));

	sscan = systable_beginscan(pg_resgroup_rel, ResGroupRsgnameIndexId, true,
							   SnapshotNow, 1, &scankey);

	tuple = systable_getnext(sscan);
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("resource group \"%s\" does not exist",
						stmt->name)));

	/*
	 * Remember the Oid, for destroying the in-memory
	 * resource group later.
	 */
	groupid = HeapTupleGetOid(tuple);

	/* cannot DROP default resource groups  */
	if (groupid == DEFAULTRESGROUP_OID || groupid == ADMINRESGROUP_OID)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot drop default resource group \"%s\"",
						stmt->name)));

	/*
	 * Check to see if any roles are in this resource group.
	 */
	authIdRel = heap_open(AuthIdRelationId, RowExclusiveLock);
	ScanKeyInit(&authid_scankey,
				Anum_pg_authid_rolresgroup,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(groupid));

	authid_scan = systable_beginscan(authIdRel, AuthIdRolResGroupIndexId, true,
									 SnapshotNow, 1, &authid_scankey);

	if (HeapTupleIsValid(systable_getnext(authid_scan)))
		ereport(ERROR,
				(errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
				 errmsg("resource group \"%s\" is used by at least one role",
						stmt->name)));

	systable_endscan(authid_scan);
	heap_close(authIdRel, RowExclusiveLock);


	/*
	 * Delete the resource group from the catalog.
	 */
	simple_heap_delete(pg_resgroup_rel, &tuple->t_self);
	systable_endscan(sscan);
	heap_close(pg_resgroup_rel, NoLock);

	/* drop the extended attributes for this resource group */
	deleteResgroupCapabilities(groupid);

	/*
	 * Remove any comments on this resource group
	 */
	DeleteSharedComments(groupid, ResGroupRelationId);

	/* metadata tracking */
	MetaTrackDropObject(ResGroupRelationId, groupid);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR|
									DF_WITH_SNAPSHOT|
									DF_NEED_TWO_PHASE,
									NIL, /* FIXME */
									NULL);
	}

	if (IsResGroupEnabled())
	{
		/*
		 * Remove the group from shared memory, this should be performed before
		 * modifying catalog, since the check of no exiting running transaction may
		 * fail
		 */
		FreeResGroupEntry(groupid, stmt->name);

		/* Argument of callback function should be allocated in heap region */
		callbackArg = (Oid *)MemoryContextAlloc(TopMemoryContext, sizeof(Oid));
		*callbackArg = groupid;
		RegisterResourceReleaseCallback(dropResGroupAbortCallback, (void *)callbackArg);
	}
}

/*
 * ALTER RESOURCE GROUP
 */
void
AlterResourceGroup(AlterResourceGroupStmt *stmt)
{
	Relation	pg_resgroup_rel;
	HeapTuple	tuple;
	ScanKeyData	scankey;
	SysScanDesc	sscan;
	Oid			groupid;
	char		concurrencyStr[16];
	char		concurrencyProposedStr[16];
	int			concurrencyVal;
	int			concurrencyProposed;
	int			newConcurrency;

	/* Permission check - only superuser can alter resource groups. */
	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("must be superuser to alter resource groups")));

	if (stmt->concurrency < RESGROUP_CONCURRENCY_UNLIMITED)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_LIMIT_VALUE),
				 errmsg("concurrency limit cannot be less than %d", RESGROUP_CONCURRENCY_UNLIMITED)));

	/*
	 * Check the pg_resgroup relation to be certain the resource group already
	 * exists.
	 */
	pg_resgroup_rel = heap_open(ResGroupRelationId, RowExclusiveLock);

	ScanKeyInit(&scankey,
				Anum_pg_resgroup_rsgname,
				BTEqualStrategyNumber, F_NAMEEQ,
				CStringGetDatum(stmt->name));

	sscan = systable_beginscan(pg_resgroup_rel, ResGroupRsgnameIndexId, true,
							   SnapshotNow, 1, &scankey);

	tuple = systable_getnext(sscan);
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("resource group \"%s\" does not exist",
						stmt->name)));

	groupid = HeapTupleGetOid(tuple);
	systable_endscan(sscan);
	heap_close(pg_resgroup_rel, NoLock);

	GetConcurrencyForResGroup(groupid, &concurrencyVal, &concurrencyProposed);
	newConcurrency = CalcConcurrencyValue(groupid, concurrencyVal, concurrencyProposed, stmt->concurrency);

	snprintf(concurrencyStr, sizeof(concurrencyStr), "%d", newConcurrency);
	snprintf(concurrencyProposedStr, sizeof(concurrencyProposedStr), "%d", stmt->concurrency);
	updateResgroupCapabilityEntry(groupid, RESGROUP_LIMIT_TYPE_CONCURRENCY, concurrencyStr, concurrencyProposedStr);

	/* Bump command counter to make this change visible in the callback function alterResGroupCommitCallback() */
	CommandCounterIncrement();

	if (IsResGroupEnabled())
	{
		Oid			*callbackArg;

		/* Argument of callback function should be allocated in heap region */
		callbackArg = (Oid *)MemoryContextAlloc(TopMemoryContext, sizeof(Oid));
		*callbackArg = groupid;
		RegisterResourceReleaseCallback(alterResGroupCommitCallback, (void *)callbackArg);
	}
}

/*
 * Get 'concurrency' of on resource group in pg_resgroupcapability.
 */
void
GetConcurrencyForResGroup(int groupId, int *value, int *proposed)
{
	char *valueStr;
	char *proposedStr;

	getResgroupCapabilityEntry(groupId, RESGROUP_LIMIT_TYPE_CONCURRENCY, &valueStr, &proposedStr);

	if (value != NULL)
		*value = pg_atoi(valueStr, sizeof(int32), 0);

	if (proposed != NULL)
		*proposed = pg_atoi(proposedStr, sizeof(int32), 0);
}

/*
 * Get resource group id for a role in pg_authid
 */
Oid
GetResGroupIdForRole(Oid roleid)
{
	HeapTuple	tuple;
	ResourceOwner owner = NULL;
	Oid			groupId;
	Relation	rel;
	ScanKeyData	key;
	SysScanDesc	 sscan;

	/*
	 * to cave the code of cache part, we provide a resource owner here if no
	 * existing
	 */
	if (CurrentResourceOwner == NULL)
	{
		owner = ResourceOwnerCreate(NULL, "GetResGroupIdForRole");
		CurrentResourceOwner = owner;
	}

	rel = heap_open(AuthIdRelationId, AccessShareLock);

	ScanKeyInit(&key,
				ObjectIdAttributeNumber,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(roleid));

	sscan = systable_beginscan(rel, AuthIdOidIndexId, true, SnapshotNow, 1, &key);

	tuple = systable_getnext(sscan);
	if (!HeapTupleIsValid(tuple))
	{
		systable_endscan(sscan);
		heap_close(rel, AccessShareLock);

		/*
		 * Role should have been dropped by other backends in this case, so this
		 * session cannot execute any command anymore
		 */
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				errmsg("Role with Oid %d was dropped", roleid),
				errhint("Cannot execute commands anymore, please terminate this session.")));
	}

	/* must access tuple before systable_endscan */
	groupId = DatumGetObjectId(heap_getattr(tuple, Anum_pg_authid_rolresgroup, rel->rd_att, NULL));

	systable_endscan(sscan);

	/*
	 * release lock here to guarantee we have no lock held when acquiring
	 * resource group slot
	 */
	heap_close(rel, AccessShareLock);

	if (!OidIsValid(groupId))
		groupId = InvalidOid;

	if (owner)
	{
		CurrentResourceOwner = NULL;
		ResourceOwnerDelete(owner);
	}

	return groupId;
}

/*
 * Get status of resource groups
 */
Datum
pg_resgroup_get_status_kv(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcontext;
		TupleDesc	tupdesc;
		int			nattr = 3;

		funcctx = SRF_FIRSTCALL_INIT();

		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		tupdesc = CreateTemplateTupleDesc(nattr, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "rsgid", OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "prop", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "value", TEXTOID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		if (PG_ARGISNULL(0))
		{
			funcctx->max_calls = 0;
		}
		else
		{
			Relation pg_resgroup_rel;
			SysScanDesc sscan;
			HeapTuple tuple;

			funcctx->user_fctx = palloc0(sizeof(Datum) * MaxResourceGroups);

			pg_resgroup_rel = heap_open(ResGroupRelationId, AccessShareLock);

			sscan = systable_beginscan(pg_resgroup_rel, InvalidOid, false,
									   SnapshotNow, 0, NULL);
			while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
			{
				Assert(funcctx->max_calls < MaxResourceGroups);
				((Datum *) funcctx->user_fctx)[funcctx->max_calls++] = ObjectIdGetDatum(HeapTupleGetOid(tuple));
			}
			systable_endscan(sscan);

			heap_close(pg_resgroup_rel, AccessShareLock);
		}

		MemoryContextSwitchTo(oldcontext);
	}

	/* stuff done on every call of the function */
	funcctx = SRF_PERCALL_SETUP();

	if (funcctx->call_cntr < funcctx->max_calls)
	{
		/* for each row */
		char *		prop = text_to_cstring(PG_GETARG_TEXT_P(0));
		Datum		values[3];
		bool		nulls[3];
		HeapTuple	tuple;
		Oid			groupId;
		char		statVal[MAXDATELEN + 1];

		MemSet(values, 0, sizeof(values));
		MemSet(nulls, 0, sizeof(nulls));
		MemSet(statVal, 0, sizeof(statVal));

		values[0] = ((Datum *) funcctx->user_fctx)[funcctx->call_cntr];
		values[1] = CStringGetTextDatum(prop);

		groupId = DatumGetObjectId(values[0]);

		/* Fill with dummy values */
		if (!strcmp(prop, "num_running"))
			ResGroupGetStat(groupId, RES_GROUP_STAT_NRUNNING, statVal, sizeof(statVal));
		else if (!strcmp(prop, "num_queueing"))
			ResGroupGetStat(groupId, RES_GROUP_STAT_NQUEUEING, statVal, sizeof(statVal));
		else if (!strcmp(prop, "cpu_usage"))
			snprintf(statVal, sizeof(statVal), "%.2f", 0.0);
		else if (!strcmp(prop, "memory_usage"))
			snprintf(statVal, sizeof(statVal), "%.2f", 0.0);
		else if (!strcmp(prop, "total_queue_duration"))
			ResGroupGetStat(groupId, RES_GROUP_STAT_TOTAL_QUEUE_TIME, statVal, sizeof(statVal));
		else if (!strcmp(prop, "num_queued"))
			ResGroupGetStat(groupId, RES_GROUP_STAT_TOTAL_QUEUED, statVal, sizeof(statVal));
		else if (!strcmp(prop, "num_executed"))
			ResGroupGetStat(groupId, RES_GROUP_STAT_TOTAL_EXECUTED, statVal, sizeof(statVal));
		else
			/* unknown property name */
			nulls[2] = true;

		if (!nulls[2])
			values[2] = CStringGetTextDatum(statVal);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);

		SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
	}
	else
	{
		/* nothing left */
		SRF_RETURN_DONE(funcctx);
	}
}

/*
 * Get the option type from a name string.
 *
 * @param defname  the name string
 *
 * @return the option type or UNKNOWN if the name is unknown
 */
static int
getResgroupOptionType(const char* defname)
{
	if (strcmp(defname, "cpu_rate_limit") == 0)
		return RESGROUP_LIMIT_TYPE_CPU;
	else if (strcmp(defname, "memory_limit") == 0)
		return RESGROUP_LIMIT_TYPE_MEMORY;
	else if (strcmp(defname, "concurrency") == 0)
		return RESGROUP_LIMIT_TYPE_CONCURRENCY;
	else if (strcmp(defname, "memory_redzone_limit") == 0)
		return RESGROUP_LIMIT_TYPE_MEMORY_REDZONE;
	else
		return RESGROUP_LIMIT_TYPE_UNKNOWN;
}

/*
 * Parse a statement and store the settings in options.
 *
 * @param stmt     the statement
 * @param options  used to store the settings
 */
static void
parseStmtOptions(CreateResourceGroupStmt *stmt, ResourceGroupOptions *options)
{
	ListCell *cell;
	int types = 0;

	foreach(cell, stmt->options)
	{
		DefElem *defel = (DefElem *) lfirst(cell);
		int type = getResgroupOptionType(defel->defname);

		if (type == RESGROUP_LIMIT_TYPE_UNKNOWN)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("option \"%s\" not recognized", defel->defname)));

		if (types & (1 << type))
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					errmsg("Find duplicate resoure group resource type: %s",
						   defel->defname)));
		else
			types |= 1 << type;

		switch (type)
		{
			case RESGROUP_LIMIT_TYPE_CONCURRENCY:
				options->concurrency = defGetInt64(defel);
				if (options->concurrency < 0 || options->concurrency > MaxConnections)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							errmsg("concurrency range is [0, MaxConnections]")));
				break;

			case RESGROUP_LIMIT_TYPE_CPU:
				options->cpuRateLimit = roundf(defGetNumeric(defel) * 100) / 100;
				if (options->cpuRateLimit <= .01f || options->cpuRateLimit >= 1.0)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							errmsg("cpu_rate_limit range is (.01, 1)")));
				break;

			case RESGROUP_LIMIT_TYPE_MEMORY:
				options->memoryLimit = roundf(defGetNumeric(defel) * 100) / 100;
				if (options->memoryLimit <= .01f || options->memoryLimit >= 1.0)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							errmsg("memory_limit range is (.01, 1)")));
				break;

			case RESGROUP_LIMIT_TYPE_MEMORY_REDZONE:
				options->redzoneLimit = roundf(defGetNumeric(defel) * 100) / 100;
				if (options->redzoneLimit <= .5f || options->redzoneLimit > 1.0)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							errmsg("memory_redzone_limit range is (.5, 1]")));
				break;

			default:
				Assert(!"unexpected options");
				break;
		}
	}

	if (options->memoryLimit == 0 || options->cpuRateLimit == 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("must specify both memory_limit and cpu_rate_limit")));

	if (options->concurrency == 0)
		options->concurrency = RESGROUP_DEFAULT_CONCURRENCY;

	if (options->redzoneLimit == 0)
		options->redzoneLimit = RESGROUP_DEFAULT_REDZONE_LIMIT;
}

/*
 * Resource owner call back function
 *
 * Remove resource group entry in shared memory when abort transaction which
 * creates resource groups
 */
static void
createResGroupAbortCallback(ResourceReleasePhase phase,
							bool isCommit, bool isTopLevel, void *arg)
{
	Oid groupId;

	if (!isTopLevel ||
		IsTransactionPreparing() ||
		phase != RESOURCE_RELEASE_BEFORE_LOCKS)
		return;

	if (!isCommit)
	{
		groupId = *(Oid *)arg;

		/*
		 * FreeResGroupEntry would acquire LWLock, since this callback is called
		 * after LWLockReleaseAll in AbortTransaction, it is safe here
		 */
		FreeResGroupEntry(groupId, NULL);
	}

	UnregisterResourceReleaseCallback(createResGroupAbortCallback, arg);
}

/*
 * Resource owner call back function
 *
 * Restore resource group entry in shared memory when abort transaction which
 * drops resource groups
 */
static void
dropResGroupAbortCallback(ResourceReleasePhase phase,
						  bool isCommit, bool isTopLevel, void *arg)
{
	Oid groupId;

	if (!isTopLevel ||
		IsTransactionPreparing() ||
		phase != RESOURCE_RELEASE_BEFORE_LOCKS)
		return;

	if (!isCommit)
	{
		groupId = *(Oid *)arg;

		/*
		 * AllocResGroupEntry would acquire LWLock, since this callback is called
		 * after LWLockReleaseAll in AbortTransaction, it is safe here
		 */
		AllocResGroupEntry(groupId);
	}

	UnregisterResourceReleaseCallback(dropResGroupAbortCallback, arg);
}

/*
 * Resource owner call back function
 *
 * When ALTER RESOURCE GROUP SET CONCURRENCY commits, some queueing
 * transaction of this resource group may need to be woke up.
 *
 */
static void
alterResGroupCommitCallback(ResourceReleasePhase phase,
						  bool isCommit, bool isTopLevel, void *arg)
{
	Oid groupId;

	if (!isTopLevel ||
		IsTransactionPreparing() ||
		phase != RESOURCE_RELEASE_BEFORE_LOCKS)
		return;

	if (isCommit)
	{
		groupId = *(Oid *)arg;

		/* wake up */
		ResGroupAlterCheckForWakeup(groupId);
	}

	UnregisterResourceReleaseCallback(alterResGroupCommitCallback, arg);
}

/*
 * Catalog access functions
 */

/*
 * Insert all the capabilities to the capability table.
 *
 * We store the capabilities in multiple lines for one group,
 * so we have to insert them one by one. This function will
 * handle the type conversion etc..
 *
 * @param groupid  oid of the resource group
 * @param options  the capabilities
 */
static void
insertResgroupCapabilities(Oid groupid,
						   ResourceGroupOptions *options)
{
	char value[64];
	Relation resgroup_capability_rel = heap_open(ResGroupCapabilityRelationId, RowExclusiveLock);

	validateCapabilities(resgroup_capability_rel, groupid, options);

	sprintf(value, "%d", options->concurrency);
	insertResgroupCapabilityEntry(resgroup_capability_rel, groupid, RESGROUP_LIMIT_TYPE_CONCURRENCY, value);

	sprintf(value, "%.2f", options->cpuRateLimit);
	insertResgroupCapabilityEntry(resgroup_capability_rel, groupid, RESGROUP_LIMIT_TYPE_CPU, value);

	sprintf(value, "%.2f", options->memoryLimit);
	insertResgroupCapabilityEntry(resgroup_capability_rel, groupid, RESGROUP_LIMIT_TYPE_MEMORY, value);

	sprintf(value, "%.2f", options->redzoneLimit);
	insertResgroupCapabilityEntry(resgroup_capability_rel, groupid, RESGROUP_LIMIT_TYPE_MEMORY_REDZONE, value);

	heap_close(resgroup_capability_rel, NoLock);
}

/*
 * Update an entry in pg_resgroupcapability
 *
 * groupid and type are the update key, value and proposed are the update value.
 */
static void
updateResgroupCapabilityEntry(Oid groupid, uint16 type, char *value, char *proposed)
{
	HeapTuple	oldTuple;
	HeapTuple	newTuple;
	SysScanDesc	sscan;
	ScanKeyData	scankey[2];
	Datum		values[Natts_pg_resgroupcapability];
	bool		isnull[Natts_pg_resgroupcapability];
	bool		repl[Natts_pg_resgroupcapability];

	Relation resgroupCapabilityRel = heap_open(ResGroupCapabilityRelationId, RowExclusiveLock);

	ScanKeyInit(&scankey[0],
				Anum_pg_resgroupcapability_resgroupid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(groupid));

	ScanKeyInit(&scankey[1],
				Anum_pg_resgroupcapability_reslimittype,
				BTEqualStrategyNumber, F_INT2EQ,
				UInt16GetDatum(type));

	sscan = systable_beginscan(resgroupCapabilityRel, ResGroupCapabilityResgroupidResLimittypeIndexId, true,
							   SnapshotNow, 2, scankey);

	if (!HeapTupleIsValid(oldTuple = systable_getnext(sscan)))
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("capability missing for resource group")));

	MemSet(isnull, 0, sizeof(bool) * Natts_pg_resgroupcapability);
	MemSet(repl, 0, sizeof(bool) * Natts_pg_resgroupcapability);

	if (value != NULL)
	{
		values[Anum_pg_resgroupcapability_value - 1] = CStringGetTextDatum(value);
		isnull[Anum_pg_resgroupcapability_value - 1] = false;
		repl[Anum_pg_resgroupcapability_value - 1]  = true;
	}

	if (proposed != NULL)
	{
		values[Anum_pg_resgroupcapability_proposed - 1] = CStringGetTextDatum(proposed);
		isnull[Anum_pg_resgroupcapability_proposed - 1] = false;
		repl[Anum_pg_resgroupcapability_proposed - 1]  = true;
	}

	newTuple = heap_modify_tuple(oldTuple, RelationGetDescr(resgroupCapabilityRel),
								  values, isnull, repl);

	simple_heap_update(resgroupCapabilityRel, &oldTuple->t_self, newTuple);
	CatalogUpdateIndexes(resgroupCapabilityRel, newTuple);

	systable_endscan(sscan);
	heap_close(resgroupCapabilityRel, NoLock);
}

/*
 * Validate the capabilities.
 *
 * The policy is resouces can't be over used, take memory for example,
 * all the allocated memory can not exceed 1.0.
 *
 * Also detect for duplicate settings for the group.
 *
 * @param rel      the relation
 * @param groupid  oid of the resource group
 * @param options  the options for the resource group
 */
static void
validateCapabilities(Relation rel,
					 Oid groupid,
					 ResourceGroupOptions *options)
{
	HeapTuple tuple;
	SysScanDesc sscan;
	float totalCpu = options->cpuRateLimit;
	float totalMem = options->memoryLimit;

	sscan = systable_beginscan(rel, InvalidOid, false, SnapshotNow, 0, NULL);

	while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
	{
		Form_pg_resgroupcapability resgCapability =
						(Form_pg_resgroupcapability)GETSTRUCT(tuple);

		if (resgCapability->resgroupid == groupid)
			ereport(ERROR,
					(errcode(ERRCODE_DUPLICATE_OBJECT),
					errmsg("Find duplicate resoure group id:%d", groupid)));

		if (resgCapability->reslimittype == RESGROUP_LIMIT_TYPE_CPU)
		{
			totalCpu += text2Float(&resgCapability->value, "cpu_rate_limit");
			if (totalCpu > 1.0)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						errmsg("total cpu_rate_limit exceeded the limit of 1.0")));
		}
		else if (resgCapability->reslimittype == RESGROUP_LIMIT_TYPE_MEMORY)
		{
			totalMem += text2Float(&resgCapability->value, "memory_limit");
			if (totalMem > 1.0)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						errmsg("total memory_limit exceeded the limit of 1.0")));
		}
	}

	systable_endscan(sscan);
}

/*
 * Insert one capability to the capability table.
 *
 * 'value' and 'proposed' are both used to describe a resource,
 * in this routine we assume 'proposed' has the same value as 'value'.
 *
 * @param rel      the relation
 * @param groupid  oid of the resource group
 * @param type     the resource limit type
 * @param value    the limit value
 */
static void
insertResgroupCapabilityEntry(Relation rel,
							 Oid groupid,
							 uint16 type,
							 char *value)
{
	Datum new_record[Natts_pg_resgroupcapability];
	bool new_record_nulls[Natts_pg_resgroupcapability];
	HeapTuple tuple;
	TupleDesc tupleDesc = RelationGetDescr(rel);

	MemSet(new_record, 0, sizeof(new_record));
	MemSet(new_record_nulls, false, sizeof(new_record_nulls));

	new_record[Anum_pg_resgroupcapability_resgroupid - 1] = ObjectIdGetDatum(groupid);
	new_record[Anum_pg_resgroupcapability_reslimittype - 1] = UInt16GetDatum(type);
	new_record[Anum_pg_resgroupcapability_value - 1] = CStringGetTextDatum(value);
	new_record[Anum_pg_resgroupcapability_proposed - 1] = CStringGetTextDatum(value);

	tuple = heap_form_tuple(tupleDesc, new_record, new_record_nulls);
	simple_heap_insert(rel, tuple);
	CatalogUpdateIndexes(rel, tuple);
}

/*
 * Retrive capability value from pg_resgroupcapability
 */
static void
getResgroupCapabilityEntry(int groupId, int type, char **value, char **proposed)
{
	SysScanDesc	sscan;
	ScanKeyData	key[2];
	HeapTuple	tuple;
	bool isNull;
	Datum valueDatum;
	Datum proposedDatum;
	Relation	relResGroupCapability;
	ResourceOwner owner = NULL;

	if (CurrentResourceOwner == NULL)
	{
		owner = ResourceOwnerCreate(NULL, "getResgroupCapabilityEntry");
		CurrentResourceOwner = owner;
	}

	relResGroupCapability = heap_open(ResGroupCapabilityRelationId, AccessShareLock);

	ScanKeyInit(&key[0],
				Anum_pg_resgroupcapability_resgroupid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(groupId));
	ScanKeyInit(&key[1],
				Anum_pg_resgroupcapability_reslimittype,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum(type));

	sscan = systable_beginscan(relResGroupCapability,
							   ResGroupCapabilityResgroupidResLimittypeIndexId,
							   true,
							   SnapshotNow, 2, key);

	tuple = systable_getnext(sscan);
	if (!HeapTupleIsValid(tuple))
	{
		systable_endscan(sscan);
		heap_close(relResGroupCapability, AccessShareLock);

		if (owner)
		{
			CurrentResourceOwner = NULL;
			ResourceOwnerDelete(owner);
		}

		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("Cannot find capability for group: %d and type: %d", groupId, type)));
	}

	valueDatum = heap_getattr(tuple, Anum_pg_resgroupcapability_value, relResGroupCapability->rd_att, &isNull);
	*value = DatumGetCString(DirectFunctionCall1(textout, valueDatum));

	proposedDatum = heap_getattr(tuple, Anum_pg_resgroupcapability_proposed, relResGroupCapability->rd_att, &isNull);
	*proposed = DatumGetCString(DirectFunctionCall1(textout, proposedDatum));

	systable_endscan(sscan);

	/*
	 * release lock here to guarantee we have no lock held when acquiring
	 * resource group slot
	 */
	heap_close(relResGroupCapability, AccessShareLock);

	if (owner)
	{
		CurrentResourceOwner = NULL;
		ResourceOwnerDelete(owner);
	}
}

/* 
 * Delete capability entries of one resource group.
 */
static void
deleteResgroupCapabilities(Oid groupid)
{
	Relation	 resgroup_capability_rel;
	HeapTuple	 tuple;
	ScanKeyData	 scankey;
	SysScanDesc	 sscan;

	resgroup_capability_rel = heap_open(ResGroupCapabilityRelationId,
										RowExclusiveLock);

	ScanKeyInit(&scankey,
				Anum_pg_resgroupcapability_resgroupid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(groupid));

	sscan = systable_beginscan(resgroup_capability_rel,
							   ResGroupCapabilityResgroupidIndexId,
							   true, SnapshotNow, 1, &scankey);

	while ((tuple = systable_getnext(sscan)) != NULL)
		simple_heap_delete(resgroup_capability_rel, &tuple->t_self);

	systable_endscan(sscan);

	heap_close(resgroup_capability_rel, NoLock);
}

/*
 * GetResGroupIdForName -- Return the Oid for a resource group name
 *
 * Notes:
 *	Used by the various admin commands to convert a user supplied group name
 *	to Oid.
 */
Oid
GetResGroupIdForName(char *name, LOCKMODE lockmode)
{
	Relation	rel;
	ScanKeyData scankey;
	SysScanDesc scan;
	HeapTuple	tuple;
	Oid			rsgid;

	rel = heap_open(ResGroupRelationId, lockmode);

	/* SELECT oid FROM pg_resgroup WHERE rsgname = :1 */
	ScanKeyInit(&scankey,
				Anum_pg_resgroup_rsgname,
				BTEqualStrategyNumber, F_NAMEEQ,
				CStringGetDatum(name));
	scan = systable_beginscan(rel, ResGroupRsgnameIndexId, true,
							  SnapshotNow, 1, &scankey);

	tuple = systable_getnext(scan);
	if (HeapTupleIsValid(tuple))
		rsgid = HeapTupleGetOid(tuple);
	else
		rsgid = InvalidOid;

	systable_endscan(scan);
	heap_close(rel, lockmode);

	return rsgid;
}

/*
 * GetResGroupNameForId -- Return the resource group name for an Oid
 */
char *
GetResGroupNameForId(Oid oid, LOCKMODE lockmode)
{
	Relation	rel;
	ScanKeyData scankey;
	SysScanDesc scan;
	HeapTuple	tuple;
	char		*name = NULL;

	rel = heap_open(ResGroupRelationId, lockmode);

	/* SELECT rsgname FROM pg_resgroup WHERE oid = :1 */
	ScanKeyInit(&scankey,
				ObjectIdAttributeNumber,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(oid));
	scan = systable_beginscan(rel, ResGroupOidIndexId, true,
							  SnapshotNow, 1, &scankey);

	tuple = systable_getnext(scan);
	if (HeapTupleIsValid(tuple))
	{
		bool isnull;
		Datum nameDatum = heap_getattr(tuple, Anum_pg_resgroup_rsgname, rel->rd_att, &isnull);
		Assert (!isnull);
		Name resGroupName = DatumGetName(nameDatum);
		name = pstrdup(NameStr(*resGroupName));
	}

	systable_endscan(scan);
	heap_close(rel, lockmode);

	return name;
}

/*
 * Convert a text to a float value.
 *
 * @param text  the text
 * @param prop  the property name
 */
static float
text2Float(const text *text, const char *prop)
{
	char *str = DatumGetCString(DirectFunctionCall1(textout,
								 PointerGetDatum(text)));
	char *end = NULL;
	double val = strtod(str, &end);

	/* both the property name and value are already checked
	 * by the syntax parser, but we'll check it again anyway for safe. */
	if (end == NULL || end == str || *end != 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("%s requires a numeric value", prop)));

	return (float) val;
}

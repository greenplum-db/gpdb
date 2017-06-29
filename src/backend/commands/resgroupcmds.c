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
#include "gp-libpq-fe.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/xact.h"
#include "catalog/heap.h"
#include "catalog/oid_dispatch.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_resgroup.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbvars.h"
#include "commands/comment.h"
#include "commands/defrem.h"
#include "commands/resgroupcmds.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/datetime.h"
#include "utils/fmgroids.h"
#include "utils/resgroup.h"
#include "utils/resgroup-ops.h"
#include "utils/resource_manager.h"
#include "utils/syscache.h"

#define RESGROUP_DEFAULT_CONCURRENCY (20)
#define RESGROUP_DEFAULT_REDZONE_LIMIT (0.8)
#define RESGROUP_DEFAULT_MEM_SHARED_QUOTA (0.2)
#define RESGROUP_DEFAULT_MEM_SPILL_RATIO (0.2)


/*
 * Internal struct to store the group settings.
 */
typedef struct ResourceGroupOptions
{
	int concurrency;
	float cpuRateLimit;
	float memoryLimit;
	float redzoneLimit;
	float memSharedQuota;
	float memSpillRatio;
} ResourceGroupOptions;

typedef struct ResourceGroupStatusRow
{
	Datum oid;

	double cpuAvgUsage;
	StringInfo memoryUsage;
} ResourceGroupStatusRow;

typedef struct ResourceGroupStatusContext
{
	ResGroupStatType type;

	int nrows;
	ResourceGroupStatusRow rows[1];
} ResourceGroupStatusContext;

/*
 * The context to pass to callback in ALTER resource group
 */
typedef struct {
	Oid		groupid;
	int		limittype;
	union {
		int		i;
		float	f;
	}	value;
	union {
		int		i;
		float	f;
	}	proposed;
} ResourceGroupAlterCallbackContext;

/*
 * The form of callbacks for resource group
 */
typedef void (*ResourceGroupCallback) (bool isCommit, void *arg);

/*
 * List of add-on callbacks for resource group related operations
 * The list is maintained as circular doubly linked.
 */
typedef struct ResourceGroupCallbackItem
{
	struct ResourceGroupCallbackItem *next;
	struct ResourceGroupCallbackItem *prev;
	ResourceGroupCallback callback;
	void *arg;
} ResourceGroupCallbackItem;

static ResourceGroupCallbackItem ResourceGroup_callbacks_head =
{
	&ResourceGroup_callbacks_head, &ResourceGroup_callbacks_head, NULL, NULL
};

static ResourceGroupCallbackItem *ResourceGroup_callbacks = &ResourceGroup_callbacks_head;

static float str2Float(const char *str, const char *prop);
static float text2Float(const text *text, const char *prop);
static int getResgroupOptionType(const char* defname);
static void parseStmtOptions(CreateResourceGroupStmt *stmt, ResourceGroupOptions *options);
static void validateCapabilities(Relation rel, Oid groupid, ResourceGroupOptions *options, bool newGroup);
static void getResgroupCapabilityEntry(int groupId, int type, char **value, char **proposed);
static void insertResgroupCapabilityEntry(Relation rel, Oid groupid, uint16 type, char *value);
static void updateResgroupCapabilityEntry(Oid groupid, uint16 type, char *value, char *proposed);
static void insertResgroupCapabilities(Oid groupid, ResourceGroupOptions *options);
static void deleteResgroupCapabilities(Oid groupid);
static void createResGroupAbortCallback(bool isCommit, void *arg);
static void dropResGroupAbortCallback(bool isCommit, void *arg);
static void alterResGroupCommitCallback(bool isCommit, void *arg);
static ResGroupStatType propNameToType(const char *name);
static void getCpuUsage(ResourceGroupStatusContext *ctx);
static void getMemoryUsage(ResourceGroupStatusContext *ctx, const char *prop);
static void registerResourceGroupCallback(ResourceGroupCallback callback, void *arg);

/*
 * Register callback functions for resource group related operations.
 *
 * At transaction end, the callback occurs post-commit or post-abort, so the
 * callback functions can only do noncritical cleanup.
 */
static void
registerResourceGroupCallback(ResourceGroupCallback callback, void *arg)
{
	ResourceGroupCallbackItem *item;

	item = (ResourceGroupCallbackItem *)
		MemoryContextAlloc(TopMemoryContext,
						   sizeof(ResourceGroupCallbackItem));
	item->callback = callback;
	item->arg = arg;

	item->prev = ResourceGroup_callbacks->prev;
	item->next = ResourceGroup_callbacks;
	item->prev->next = item;
	item->next->prev = item;
}

/*
 * Call resource group related callback functions at transaction end.
 *
 * On COMMIT, the callback functions are processed as FIFO.
 * On ABORT,  the callback functions are processed as LIFO.
 *
 * Note the callback functions would be removed as being processed.
 */
void
AtEOXact_ResGroup(bool isCommit)
{
	ResourceGroupCallbackItem *current =
		isCommit ? ResourceGroup_callbacks->next : ResourceGroup_callbacks->prev;
	while (current != ResourceGroup_callbacks)
	{
		ResourceGroupCallbackItem *tmp = isCommit? current->next : current->prev;

		ResourceGroupCallback callback = current->callback;
		void *arg =  current->arg;

		current->prev->next = current->next;
		current->next->prev = current->prev;
		pfree(current);
		current = tmp;

		callback(isCommit, arg);
	}
}

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

	/* Subtransaction is not supported for resource group related operations */
	if (IsSubTransaction())
		ereport(ERROR,
				(errcode(ERRCODE_ACTIVE_SQL_TRANSACTION),
				 errmsg("CREATE RESOURCE GROUP cannot run inside a subtransaction")));

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

		/* Create os dependent part for this resource group */

		ResGroupOps_CreateGroup(groupid);
		ResGroupOps_SetCpuRateLimit(groupid, options.cpuRateLimit);

		/* Argument of callback function should be allocated in heap region */
		callbackArg = (Oid *)MemoryContextAlloc(TopMemoryContext, sizeof(Oid));
		*callbackArg = groupid;
		registerResourceGroupCallback(createResGroupAbortCallback, (void *)callbackArg);
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

	/* Subtransaction is not supported for resource group related operations */
	if (IsSubTransaction())
		ereport(ERROR,
				(errcode(ERRCODE_ACTIVE_SQL_TRANSACTION),
				 errmsg("DROP RESOURCE GROUP cannot run inside a subtransaction")));

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
		ResGroupCheckForDrop(groupid, stmt->name);

		/* Argument of callback function should be allocated in heap region */
		callbackArg = (Oid *)MemoryContextAlloc(TopMemoryContext, sizeof(Oid));
		*callbackArg = groupid;
		registerResourceGroupCallback(dropResGroupAbortCallback, (void *)callbackArg);
	}
}

/*
 * ALTER RESOURCE GROUP
 */
void
AlterResourceGroup(AlterResourceGroupStmt *stmt)
{
	Relation	pg_resgroup_rel;
	Relation	resgroup_capability_rel;
	HeapTuple	tuple;
	ScanKeyData	scankey;
	SysScanDesc	sscan;
	Oid			groupid;
	ResourceGroupAlterCallbackContext *callbackCtx;
	char		valueStr[16];
	char		proposedStr[16];
	int			concurrency;
	int			concurrencyVal;
	int			concurrencyProposed;
	int			newConcurrency;
	float		cpuRateLimitVal;
	float		cpuRateLimitNew;
	DefElem		*defel;
	int			limitType;
	bool		needDispatch = true;

	/* Permission check - only superuser can alter resource groups. */
	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("must be superuser to alter resource groups")));

	/* Subtransaction is not supported for resource group related operations */
	if (IsSubTransaction())
		ereport(ERROR,
				(errcode(ERRCODE_ACTIVE_SQL_TRANSACTION),
				 errmsg("ALTER RESOURCE GROUP cannot run inside a subtransaction")));

	/* Currently we only support to ALTER one limit at one time */
	Assert(list_length(stmt->options) == 1);
	defel = (DefElem *) lfirst(list_head(stmt->options));

	limitType = getResgroupOptionType(defel->defname);

	switch (limitType)
	{
		case RESGROUP_LIMIT_TYPE_CONCURRENCY:
			concurrency = defGetInt64(defel);
			if (concurrency < RESGROUP_CONCURRENCY_UNLIMITED)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_LIMIT_VALUE),
						 errmsg("concurrency limit cannot be less than %d",
								RESGROUP_CONCURRENCY_UNLIMITED)));
			break;

		case RESGROUP_LIMIT_TYPE_CPU:
			cpuRateLimitNew = defGetNumeric(defel);
			if (cpuRateLimitNew <= .01f)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_LIMIT_VALUE),
						 errmsg("cpu rate limit must be greater than 0.01")));
			if (cpuRateLimitNew >= 1.0)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_LIMIT_VALUE),
						 errmsg("cpu rate limit must be less than 1.00")));
			/* overall limit will be verified later after groupid is known */
			break;

		default:
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("unsupported resource group limit type '%s'", defel->defname)));
	}

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

	/* Argument of callback function should be allocated in heap region */
	callbackCtx = (ResourceGroupAlterCallbackContext *)
		MemoryContextAlloc(TopMemoryContext, sizeof(*callbackCtx));
	callbackCtx->groupid = groupid;
	callbackCtx->limittype = limitType;

	switch (limitType)
	{
		case RESGROUP_LIMIT_TYPE_CONCURRENCY:
			GetConcurrencyForResGroup(groupid,
									  &concurrencyVal,
									  &concurrencyProposed);
			newConcurrency = CalcConcurrencyValue(groupid,
												  concurrencyVal,
												  concurrencyProposed,
												  concurrency);


			snprintf(valueStr, sizeof(valueStr), "%d", newConcurrency);
			snprintf(proposedStr, sizeof(proposedStr), "%d", concurrency);
			updateResgroupCapabilityEntry(groupid, limitType, valueStr, proposedStr);

			callbackCtx->value.i = newConcurrency;
			callbackCtx->proposed.i = concurrency;

			needDispatch = true;
			break;

		case RESGROUP_LIMIT_TYPE_CPU:
			cpuRateLimitVal = GetCpuRateLimitForResGroup(groupid);
			cpuRateLimitVal = roundf(cpuRateLimitVal * 100) / 100;

			if (cpuRateLimitVal < cpuRateLimitNew)
			{
				ResourceGroupOptions options;

				options.concurrency = 0;
				options.cpuRateLimit = cpuRateLimitNew;
				options.memoryLimit = 0;
				options.redzoneLimit = 0;

				/*
				 * In validateCapabilities() we scan all the resource groups
				 * to check whether the total cpu_rate_limit exceed 1.0 or not.
				 * We need to use ExclusiveLock here to prevent concurrent
				 * increase on different resource group.
				 */
				resgroup_capability_rel = heap_open(ResGroupCapabilityRelationId,
													ExclusiveLock);
				validateCapabilities(resgroup_capability_rel,
									 groupid, &options, false);
				heap_close(resgroup_capability_rel, NoLock);
			}

			snprintf(valueStr, sizeof(valueStr),
					 "%.2f", cpuRateLimitNew);
			snprintf(proposedStr, sizeof(proposedStr),
					 "%.2f", cpuRateLimitNew);
			updateResgroupCapabilityEntry(groupid, limitType,
										  valueStr, proposedStr);

			callbackCtx->value.f = cpuRateLimitNew;
			callbackCtx->proposed.f = cpuRateLimitNew;
			break;

		default:
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("unsupported resource group limit type '%s'", defel->defname)));
	}

	if (needDispatch && Gp_role == GP_ROLE_DISPATCH)
	{
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR|
									DF_WITH_SNAPSHOT|
									DF_NEED_TWO_PHASE,
									GetAssignedOidsForDispatch(), /* FIXME */
									NULL);
	}

	/* Bump command counter to make this change visible in the callback function alterResGroupCommitCallback() */
	CommandCounterIncrement();

	if (IsResGroupEnabled())
	{
		registerResourceGroupCallback(alterResGroupCommitCallback, (void *)callbackCtx);
	}
}

/*
 * Get 'concurrency' of one resource group in pg_resgroupcapability.
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
 * Get 'cpu_rate_limit' of one resource group in pg_resgroupcapability.
 */
float
GetCpuRateLimitForResGroup(int groupId)
{
	char *valueStr;
	char *proposedStr;

	getResgroupCapabilityEntry(groupId, RESGROUP_LIMIT_TYPE_CPU,
							   &valueStr, &proposedStr);

	return str2Float(valueStr, "cpu_rate_limit");
}

/*
 * Get memory capabilities of one resource group in pg_resgroupcapability.
 * TODO: scan the catalog table only once?
 */
void
GetMemoryCapabilitiesForResGroup(int groupId, float *memoryLimit, float *sharedQuota, float *spillRatio)
{
	char *valueStr;
	char *proposedStr;

	getResgroupCapabilityEntry(groupId, RESGROUP_LIMIT_TYPE_MEMORY,
							   &valueStr, &proposedStr);
	*memoryLimit = str2Float(valueStr, "memory_limit");

	getResgroupCapabilityEntry(groupId, RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA,
							   &valueStr, &proposedStr);
	*sharedQuota = str2Float(valueStr, "memory_shared_quota");

	getResgroupCapabilityEntry(groupId, RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO,
							   &valueStr, &proposedStr);
	*spillRatio = str2Float(valueStr, "memory_spill_ratio");
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
 * Convert from property name to ResGroupStatType.
 */
static ResGroupStatType
propNameToType(const char *name)
{
	if (!strcmp(name, "num_running"))
		return RES_GROUP_STAT_NRUNNING;
	else if (!strcmp(name, "num_queueing"))
		return RES_GROUP_STAT_NQUEUEING;
	else if (!strcmp(name, "cpu_usage"))
		return RES_GROUP_STAT_CPU_USAGE;
	else if (!strcmp(name, "memory_usage"))
		return RES_GROUP_STAT_MEM_USAGE;
	else if (!strcmp(name, "total_queue_duration"))
		return RES_GROUP_STAT_TOTAL_QUEUE_TIME;
	else if (!strcmp(name, "num_queued"))
		return RES_GROUP_STAT_TOTAL_QUEUED;
	else if (!strcmp(name, "num_executed"))
		return RES_GROUP_STAT_TOTAL_EXECUTED;
	else
		return RES_GROUP_STAT_UNKNOWN;
}

/*
 * Get cpu usage.
 *
 * On QD this function dispatch the request to all QEs, collecting both
 * QEs' and QD's cpu usage and calculate the average.
 *
 * On QE this function only collect the cpu usage on itself.
 *
 * Cpu usage is a ratio within [0%, 100%], however due to error the actual
 * value might be greater than 100%, that's not a bug.
 */
static void
getCpuUsage(ResourceGroupStatusContext *ctx)
{
	int64 *usages;
	TimestampTz *timestamps;
	int nsegs = 1;
	int ncores;
	int i, j;

	if (!IsResGroupEnabled())
		return;

	usages = palloc(sizeof(*usages) * ctx->nrows);
	timestamps = palloc(sizeof(*timestamps) * ctx->nrows);

	ncores = ResGroupOps_GetCpuCores();

	for (j = 0; j < ctx->nrows; j++)
	{
		ResourceGroupStatusRow *row = &ctx->rows[j];
		Oid rsgid = DatumGetObjectId(row->oid);

		usages[j] = ResGroupOps_GetCpuUsage(rsgid);
		timestamps[j] = GetCurrentTimestamp();
	}

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CdbPgResults cdb_pgresults = {NULL, 0};
		StringInfoData buffer;

		initStringInfo(&buffer);
		appendStringInfo(&buffer, "SELECT rsgid, value FROM pg_resgroup_get_status_kv('cpu_usage')");

		CdbDispatchCommand(buffer.data, DF_WITH_SNAPSHOT, &cdb_pgresults);

		if (cdb_pgresults.numResults == 0)
			elog(ERROR, "gp_resgroup_status didn't get back any cpu usage statistics from the segDBs");

		nsegs += cdb_pgresults.numResults;

		for (i = 0; i < cdb_pgresults.numResults; i++)
		{
			struct pg_result *pg_result = cdb_pgresults.pg_results[i];

			/*
			 * Any error here should have propagated into errbuf, so we shouldn't
			 * ever see anything other that tuples_ok here.  But, check to be
			 * sure.
			 */
			if (PQresultStatus(pg_result) != PGRES_TUPLES_OK)
			{
				cdbdisp_clearCdbPgResults(&cdb_pgresults);
				elog(ERROR, "gp_resgroup_status: resultStatus not tuples_Ok");
			}
			else
			{
				Assert(PQntuples(pg_result) == ctx->nrows);
				for (j = 0; j < ctx->nrows; j++)
				{
					double usage;
					const char *result;
					ResourceGroupStatusRow *row = &ctx->rows[j];
					Oid rsgid = pg_atoi(PQgetvalue(pg_result, j, 0),
										sizeof(Oid), 0);
					/*
					 * we assume QD and QE shall have the same order
					 * for all the resgroups, but in case this assumption
					 * failed we do a full lookup
					 */
					if (rsgid != DatumGetObjectId(row->oid))
					{
						int k;
						for (k = 0; k < ctx->nrows; k++)
						{
							row = &ctx->rows[k];
							if (rsgid == DatumGetObjectId(row->oid))
								break;
						}
						if (k == ctx->nrows)
							elog(ERROR, "gp_resgroup_status: inconsistent resgroups between QD and QE");
					}

					result = PQgetvalue(pg_result, j, 1);
					sscanf(result, "%lf", &usage);

					row->cpuAvgUsage += usage;
				}
			}
		}

		cdbdisp_clearCdbPgResults(&cdb_pgresults);
	}
	else
	{
		pg_usleep(300000);
	}

	for (j = 0; j < ctx->nrows; j++)
	{
		int64 duration;
		long secs;
		int usecs;
		int64 usage;
		ResourceGroupStatusRow *row = &ctx->rows[j];
		Oid rsgid = DatumGetObjectId(row->oid);

		usage = ResGroupOps_GetCpuUsage(rsgid) - usages[j];

		TimestampDifference(timestamps[j], GetCurrentTimestamp(),
							&secs, &usecs);

		duration = secs * 1000000 + usecs;

		/*
		 * usage is the cpu time (nano seconds) obtained by this group
		 * in the time duration (micro seconds), so cpu time on one core
		 * can be calculated as:
		 *
		 *     usage / 1000 / duration / ncores
		 *
		 * To convert it to percentange we should multiple 100%.
		 */
		row->cpuAvgUsage += usage / 10.0 / duration / ncores;
		row->cpuAvgUsage /= nsegs;
	}
}

/*
 * Get memory usage.
 *
 * On QD this function dispatch the request to all QEs, collecting both
 * QEs' and QD's memory usage.
 *
 * On QE this function only collect the memory usage on itself.
 *
 * Memory usage is returned in JSON format.
 */
static void
getMemoryUsage(ResourceGroupStatusContext *ctx, const char *prop)
{
	int i, j;

	if (!IsResGroupEnabled())
		return;

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CdbPgResults cdb_pgresults = {NULL, 0};
		StringInfoData buffer;

		initStringInfo(&buffer);
		appendStringInfo(&buffer, "SELECT rsgid, value FROM pg_resgroup_get_status_kv('memory_usage') order by rsgid");

		CdbDispatchCommand(buffer.data, DF_WITH_SNAPSHOT, &cdb_pgresults);

		if (cdb_pgresults.numResults == 0)
			elog(ERROR, "gp_resgroup_status didn't get back any memory usage statistics from the segDBs");

		for (i = 0; i < cdb_pgresults.numResults; i++)
		{
			struct pg_result *pg_result = cdb_pgresults.pg_results[i];

			/*
			 * Any error here should have propagated into errbuf, so we shouldn't
			 * ever see anything other that tuples_ok here.  But, check to be
			 * sure.
			 */
			if (PQresultStatus(pg_result) != PGRES_TUPLES_OK)
			{
				cdbdisp_clearCdbPgResults(&cdb_pgresults);
				elog(ERROR, "gp_resgroup_status: resultStatus not tuples_Ok");
			}

			Assert(PQntuples(pg_result) == ctx->nrows);
			for (j = 0; j < ctx->nrows; j++)
			{
				const char *result;
				ResourceGroupStatusRow *row = &ctx->rows[j];
				Oid rsgid = pg_atoi(PQgetvalue(pg_result, j, 0), sizeof(Oid), 0);

				if (row->memoryUsage->len == 0)
				{
					char statVal[MAXDATELEN + 1];

					row->oid = rsgid;
					ResGroupGetStat(rsgid, RES_GROUP_STAT_MEM_USAGE, statVal, sizeof(statVal), prop);
					appendStringInfo(row->memoryUsage, "{\"%d\":%s", GpIdentity.segindex, statVal);
				}

				result = PQgetvalue(pg_result, j, 1);
				appendStringInfo(row->memoryUsage, ", %s", result);

				if (i == cdb_pgresults.numResults - 1)
					appendStringInfoChar(row->memoryUsage, '}');
			}
		}

		cdbdisp_clearCdbPgResults(&cdb_pgresults);
	}
	else
	{
		for (j = 0; j < ctx->nrows; j++)
		{
			char statVal[MAXDATELEN + 1];
			ResourceGroupStatusRow *row = &ctx->rows[j];
			Oid groupId = DatumGetObjectId(row->oid);

			ResGroupGetStat(groupId, RES_GROUP_STAT_MEM_USAGE, statVal, sizeof(statVal), prop);
			appendStringInfo(row->memoryUsage, "\"%d\":%s", GpIdentity.segindex, statVal);
		}
	}
}

/*
 * Get status of resource groups
 */
Datum
pg_resgroup_get_status_kv(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	ResourceGroupStatusContext *ctx;

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
			char *		prop = text_to_cstring(PG_GETARG_TEXT_P(0));

			int ctxsize = sizeof(ResourceGroupStatusContext) +
				sizeof(ResourceGroupStatusRow) * (MaxResourceGroups - 1);

			funcctx->user_fctx = palloc(ctxsize);
			ctx = (ResourceGroupStatusContext *) funcctx->user_fctx;

			pg_resgroup_rel = heap_open(ResGroupRelationId, AccessShareLock);

			sscan = systable_beginscan(pg_resgroup_rel, InvalidOid, false,
									   SnapshotNow, 0, NULL);
			while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
			{
				Assert(funcctx->max_calls < MaxResourceGroups);
				ctx->rows[funcctx->max_calls].cpuAvgUsage = 0;
				ctx->rows[funcctx->max_calls].memoryUsage = makeStringInfo();
				ctx->rows[funcctx->max_calls++].oid =
					ObjectIdGetDatum(HeapTupleGetOid(tuple));
			}
			systable_endscan(sscan);

			heap_close(pg_resgroup_rel, AccessShareLock);

			ctx->nrows = funcctx->max_calls;
			ctx->type = propNameToType(prop);
			switch (ctx->type)
			{
				case RES_GROUP_STAT_CPU_USAGE:
					getCpuUsage(ctx);
					break;
				case RES_GROUP_STAT_MEM_USAGE:
					getMemoryUsage(ctx, prop);
					break;
				default:
					break;
			}
		}

		MemoryContextSwitchTo(oldcontext);
	}

	/* stuff done on every call of the function */
	funcctx = SRF_PERCALL_SETUP();
	ctx = (ResourceGroupStatusContext *) funcctx->user_fctx;

	if (funcctx->call_cntr < funcctx->max_calls)
	{
		/* for each row */
		char *		prop = text_to_cstring(PG_GETARG_TEXT_P(0));
		Datum		values[3];
		bool		nulls[3];
		HeapTuple	tuple;
		Oid			groupId;
		char		statVal[MAXDATELEN + 1];
		ResourceGroupStatusRow *row = &ctx->rows[funcctx->call_cntr];

		MemSet(values, 0, sizeof(values));
		MemSet(nulls, 0, sizeof(nulls));
		MemSet(statVal, 0, sizeof(statVal));

		values[0] = row->oid;
		values[1] = CStringGetTextDatum(prop);

		groupId = DatumGetObjectId(values[0]);

		switch (ctx->type)
		{
			default:
			case RES_GROUP_STAT_NRUNNING:
			case RES_GROUP_STAT_NQUEUEING:
			case RES_GROUP_STAT_TOTAL_EXECUTED:
			case RES_GROUP_STAT_TOTAL_QUEUED:
			case RES_GROUP_STAT_TOTAL_QUEUE_TIME:
				ResGroupGetStat(groupId, ctx->type, statVal, sizeof(statVal), prop);
				values[2] = CStringGetTextDatum(statVal);
				break;

			case RES_GROUP_STAT_CPU_USAGE:
				snprintf(statVal, sizeof(statVal), "%.2lf%%",
						 row->cpuAvgUsage);
				values[2] = CStringGetTextDatum(statVal);
				break;

			case RES_GROUP_STAT_MEM_USAGE:
				if (IsResGroupEnabled())
					values[2] = CStringGetTextDatum(row->memoryUsage->data);
				else
					values[2] = CStringGetTextDatum("{}");
				break;
		}

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
	else if (strcmp(defname, "memory_shared_quota") == 0)
		return RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA;
	else if (strcmp(defname, "memory_spill_ratio") == 0)
		return RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO;
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

			case RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA:
				options->memSharedQuota = roundf(defGetNumeric(defel) * 100) / 100;
				break;

			case RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO:
				options->memSpillRatio = roundf(defGetNumeric(defel) * 100) / 100;
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

	if (!(types & (1 << RESGROUP_LIMIT_TYPE_CONCURRENCY)))
		options->concurrency = RESGROUP_DEFAULT_CONCURRENCY;

	if (!(types & (1 << RESGROUP_LIMIT_TYPE_MEMORY_REDZONE)))
		options->redzoneLimit = RESGROUP_DEFAULT_REDZONE_LIMIT;

	if (!(types & (1 << RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA)))
		options->memSharedQuota = RESGROUP_DEFAULT_MEM_SHARED_QUOTA;

	if (!(types & (1 << RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO)))
		options->memSpillRatio = RESGROUP_DEFAULT_MEM_SPILL_RATIO;

	if (options->memSpillRatio + options->memSharedQuota > 1.f)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("The sum of memory_shared_quota (%.2f) and memory_spill_ratio (%.2f) exceeds 1.0",
						options->memSharedQuota, options->memSpillRatio)));
}

/*
 * Resource group call back function
 *
 * Remove resource group entry in shared memory when abort transaction which
 * creates resource groups
 */
static void
createResGroupAbortCallback(bool isCommit, void *arg)
{
	Oid groupId;

	if (!isCommit)
	{
		groupId = *(Oid *)arg;

		/*
		 * FreeResGroupEntry would acquire LWLock, since this callback is called
		 * after LWLockReleaseAll in AbortTransaction, it is safe here
		 */
		FreeResGroupEntry(groupId);

		/* remove the os dependent part for this resource group */
		ResGroupOps_DestroyGroup(groupId);
	}

	pfree(arg);
}

/*
 * Resource group call back function
 *
 * Remove resource group entry in shared memory when commit transaction which
 * drops resource groups
 */
static void
dropResGroupAbortCallback(bool isCommit, void *arg)
{
	Oid groupId;

	groupId = *(Oid *)arg;
	ResGroupDropCheckForWakeup(groupId, isCommit);

	if (isCommit)
	{
		/* remove the os dependent part for this resource group */
		ResGroupOps_DestroyGroup(groupId);
	}

	pfree(arg);
}

/*
 * Resource group call back function
 *
 * When ALTER RESOURCE GROUP SET CONCURRENCY commits, some queueing
 * transaction of this resource group may need to be woke up.
 *
 */
static void
alterResGroupCommitCallback(bool isCommit, void *arg)
{
	volatile int savedInterruptHoldoffCount;
	ResourceGroupAlterCallbackContext *ctx =
		(ResourceGroupAlterCallbackContext *) arg;

	if (!isCommit)
	{
		pfree(arg);
		return;
	}

	switch (ctx->limittype)
	{
		case RESGROUP_LIMIT_TYPE_CONCURRENCY:
			/* wake up */
			ResGroupAlterCheckForWakeup(ctx->groupid,
										ctx->value.i,
										ctx->proposed.i);
			break;

		case RESGROUP_LIMIT_TYPE_CPU:
			/*
			 * Apply the cpu rate limit to cgroup.
			 *
			 * This operation can fail in some cases, e.g.:
			 * 1. BEGIN;
			 * 2. CREATE RESOURCE GROUP g1 ...;
			 * 3. ALTER RESOURCE GROUP g1 SET CPU_RATE_LIMIT ...;
			 * 4. DROP RESOURCE GROUP g1;
			 * 5. COMMIT; -- or ABORT;
			 *
			 * So the error needs to be catched here.
			 */
			PG_TRY();
			{
				savedInterruptHoldoffCount = InterruptHoldoffCount;
				ResGroupOps_SetCpuRateLimit(ctx->groupid, ctx->value.f);
			}
			PG_CATCH();
			{
				InterruptHoldoffCount = savedInterruptHoldoffCount;
				elog(LOG, "Fail to set cpu_rate_limit for resource group %d", ctx->groupid);
			}
			PG_END_TRY();
			break;

		default:
			break;
	}

	pfree(arg);
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

	validateCapabilities(resgroup_capability_rel, groupid, options, true);

	sprintf(value, "%d", options->concurrency);
	insertResgroupCapabilityEntry(resgroup_capability_rel, groupid, RESGROUP_LIMIT_TYPE_CONCURRENCY, value);

	sprintf(value, "%.2f", options->cpuRateLimit);
	insertResgroupCapabilityEntry(resgroup_capability_rel, groupid, RESGROUP_LIMIT_TYPE_CPU, value);

	sprintf(value, "%.2f", options->memoryLimit);
	insertResgroupCapabilityEntry(resgroup_capability_rel, groupid, RESGROUP_LIMIT_TYPE_MEMORY, value);

	sprintf(value, "%.2f", options->redzoneLimit);
	insertResgroupCapabilityEntry(resgroup_capability_rel, groupid, RESGROUP_LIMIT_TYPE_MEMORY_REDZONE, value);

	sprintf(value, "%.2f", options->memSharedQuota);
	insertResgroupCapabilityEntry(resgroup_capability_rel, groupid, RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA, value);

	sprintf(value, "%.2f", options->memSpillRatio);
	insertResgroupCapabilityEntry(resgroup_capability_rel, groupid, RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO, value);

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
					 ResourceGroupOptions *options,
					 bool newGroup)
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
		{
			if (!newGroup)
				continue;

			ereport(ERROR,
					(errcode(ERRCODE_DUPLICATE_OBJECT),
					errmsg("Find duplicate resoure group id:%d", groupid)));
		}

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
		char buf[64];

		systable_endscan(sscan);
		heap_close(relResGroupCapability, AccessShareLock);

		if (owner)
		{
			CurrentResourceOwner = NULL;
			ResourceOwnerDelete(owner);
		}

		/* for backward compatibility */
		switch (type)
		{
			case RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA:
				snprintf(buf, sizeof(buf), "%.2f", RESGROUP_DEFAULT_MEM_SHARED_QUOTA);
				*value = pstrdup(buf);
				*proposed = pstrdup(buf);
				return;

			case RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO:
				snprintf(buf, sizeof(buf), "%.2f", RESGROUP_DEFAULT_MEM_SPILL_RATIO);
				*value = pstrdup(buf);
				*proposed = pstrdup(buf);
				return;
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
 * Convert a C str to a float value.
 *
 * @param str   the C str
 * @param prop  the property name
 */
static float
str2Float(const char *str, const char *prop)
{
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

	return str2Float(str, prop);
}

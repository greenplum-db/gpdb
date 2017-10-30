/*-------------------------------------------------------------------------
 *
 * resgroupcmds.c
 *	  Commands for manipulating resource group.
 *
 * Portions Copyright (c) 2006-2017, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 * IDENTIFICATION
 *    src/backend/commands/resgroupcmds.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "funcapi.h"
#include "libpq-fe.h"
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
#include "utils/resowner.h"
#include "utils/syscache.h"

#define RESGROUP_DEFAULT_CONCURRENCY (20)
#define RESGROUP_DEFAULT_MEM_SHARED_QUOTA (20)
#define RESGROUP_DEFAULT_MEM_SPILL_RATIO (20)

#define RESGROUP_MIN_CONCURRENCY	(0)
#define RESGROUP_MAX_CONCURRENCY	(MaxConnections)

#define RESGROUP_MIN_CPU_RATE_LIMIT	(1)
#define RESGROUP_MAX_CPU_RATE_LIMIT	(100)

#define RESGROUP_MIN_MEMORY_LIMIT	(1)

#define RESGROUP_MIN_MEMORY_SHARED_QUOTA	(0)
#define RESGROUP_MAX_MEMORY_SHARED_QUOTA	(100)

#define RESGROUP_MIN_MEMORY_SPILL_RATIO		(0)
#define RESGROUP_MAX_MEMORY_SPILL_RATIO		(100)

/*
 * The context to pass to callback in ALTER resource group
 */
typedef struct {
	Oid		groupid;
	int		limittype;
	ResGroupCaps	caps;
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

static int str2Int(const char *str, const char *prop);
static ResGroupLimitType getResgroupOptionType(const char* defname);
static const char * getResgroupOptionName(ResGroupLimitType type);
static void parseStmtOptions(CreateResourceGroupStmt *stmt, ResGroupOpts *options);
static void validateCapabilities(Relation rel, Oid groupid, ResGroupOpts *options, bool newGroup);
static void insertResgroupCapabilityEntry(Relation rel, Oid groupid, uint16 type, char *value);
static void updateResgroupCapabilities(Oid groupid, const ResGroupCaps *resgroupCaps);
static void insertResgroupCapabilities(Oid groupid, ResGroupOpts *options);
static void deleteResgroupCapabilities(Oid groupid);
static void createResGroupCallback(bool isCommit, void *arg);
static void dropResGroupCallback(bool isCommit, void *arg);
static void alterResGroupCallback(bool isCommit, void *arg);
static void registerResourceGroupCallback(ResourceGroupCallback callback, void *arg);

/*
 * Register callback functions for resource group related operations.
 *
 * At transaction end, the callback occurs post-commit or post-abort, so the
 * callback functions can only do non-critical cleanup.
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
	ResGroupOpts options;
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
	sscan = systable_beginscan(pg_resgroup_rel, ResGroupRsgnameIndexId, false,
							   SnapshotNow, 0, NULL);
	nResGroups = 0;
	while (systable_getnext(sscan) != NULL)
		nResGroups++;
	systable_endscan(sscan);

	if (nResGroups >= MaxResourceGroups)
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
				 errmsg("insufficient resource groups available")));

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
	if (IsResGroupActivated())
	{
		Oid			*callbackArg;

		AllocResGroupEntry(groupid, &options);


		/* Argument of callback function should be allocated in heap region */
		callbackArg = (Oid *)MemoryContextAlloc(TopMemoryContext, sizeof(Oid));
		*callbackArg = groupid;
		registerResourceGroupCallback(createResGroupCallback, (void *)callbackArg);

		/* Create os dependent part for this resource group */
		ResGroupOps_CreateGroup(groupid);
		ResGroupOps_SetCpuRateLimit(groupid, options.cpuRateLimit);
	}
	else if (Gp_role == GP_ROLE_DISPATCH)
		ereport(WARNING,
				(errmsg("resource group is disabled"),
				 errhint("To enable set gp_resource_manager=group")));
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

	/* check before dispatch to segment */
	if (IsResGroupActivated())
		ResGroupCheckForDrop(groupid, stmt->name);

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
									NIL,
									NULL);
	}

	if (IsResGroupActivated())
	{
		/* Argument of callback function should be allocated in heap region */
		callbackArg = (Oid *)MemoryContextAlloc(TopMemoryContext, sizeof(Oid));
		*callbackArg = groupid;
		registerResourceGroupCallback(dropResGroupCallback, (void *)callbackArg);
	}
}

/*
 * ALTER RESOURCE GROUP
 */
void
AlterResourceGroup(AlterResourceGroupStmt *stmt)
{
	Relation	pg_resgroup_rel;
	Relation	pg_resgroupcapability_rel;
	HeapTuple	tuple;
	ScanKeyData	scankey;
	SysScanDesc	sscan;
	Oid			groupid;
	ResourceGroupAlterCallbackContext *callbackCtx;
	int			concurrency = -1;
	int			cpuRateLimitNew = -1;
	int			memSharedQuotaNew = -1;
	int			memSpillRatioNew = -1;
	int			memLimitNew = -1;
	DefElem		*defel;
	ResGroupLimitType	limitType;
	ResGroupCaps		caps;
	ResGroupOpts		opts;

	/* Permission check - only superuser can alter resource groups. */
	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("must be superuser to alter resource groups")));

	/* Currently we only support to ALTER one limit at one time */
	Assert(list_length(stmt->options) == 1);
	defel = (DefElem *) lfirst(list_head(stmt->options));

	limitType = getResgroupOptionType(defel->defname);

	switch (limitType)
	{
		case RESGROUP_LIMIT_TYPE_CONCURRENCY:
			concurrency = defGetInt64(defel);
			if (concurrency < RESGROUP_MIN_CONCURRENCY)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_LIMIT_VALUE),
						 errmsg("concurrency limit cannot be less than %d",
								RESGROUP_MIN_CONCURRENCY)));
			if (concurrency > RESGROUP_MAX_CONCURRENCY)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_LIMIT_VALUE),
						 errmsg("concurrency limit cannot be greater than 'max_connections'")));
			break;

		case RESGROUP_LIMIT_TYPE_CPU:
			cpuRateLimitNew = defGetInt64(defel);
			if (cpuRateLimitNew < RESGROUP_MIN_CPU_RATE_LIMIT)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_LIMIT_VALUE),
						 errmsg("cpu rate limit cannot be less than %d",
								RESGROUP_MIN_CPU_RATE_LIMIT)));
			if (cpuRateLimitNew > RESGROUP_MAX_CPU_RATE_LIMIT)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_LIMIT_VALUE),
						 errmsg("cpu rate limit cannot be greater than %d",
								RESGROUP_MAX_CPU_RATE_LIMIT)));
			/* overall limit will be verified later after groupid is known */
			break;

		case RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA:
			memSharedQuotaNew = defGetInt64(defel);
			if (memSharedQuotaNew < RESGROUP_MIN_MEMORY_SHARED_QUOTA)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_LIMIT_VALUE),
						 errmsg("memory shared quota cannot be less than %d",
								RESGROUP_MIN_MEMORY_SHARED_QUOTA)));
			if (memSharedQuotaNew > RESGROUP_MAX_MEMORY_SHARED_QUOTA)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_LIMIT_VALUE),
						 errmsg("memory shared quota cannot be greater than %d",
								RESGROUP_MAX_MEMORY_SHARED_QUOTA)));
			break;

		case RESGROUP_LIMIT_TYPE_MEMORY:
			memLimitNew = defGetInt64(defel);
			if (memLimitNew < RESGROUP_MIN_MEMORY_LIMIT)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_LIMIT_VALUE),
						 errmsg("memory limit cannot be less than %d",
								RESGROUP_MIN_MEMORY_LIMIT)));
			if (memLimitNew > RESGROUP_MAX_MEMORY_LIMIT)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_LIMIT_VALUE),
						 errmsg("memory limit cannot be greater than %d",
								RESGROUP_MAX_MEMORY_LIMIT)));
			/* overall limit will be verified later after groupid is known */
			break;
		case RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO:
			memSpillRatioNew = defGetInt64(defel);
			if (memSpillRatioNew < RESGROUP_MIN_MEMORY_SPILL_RATIO)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_LIMIT_VALUE),
						 errmsg("memory spill ratio cannot be less than %d",
								RESGROUP_MIN_MEMORY_SPILL_RATIO)));
			if (memSpillRatioNew > RESGROUP_MAX_MEMORY_SPILL_RATIO)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_LIMIT_VALUE),
						 errmsg("memory spill ratio cannot be greater than %d",
								RESGROUP_MAX_MEMORY_SPILL_RATIO)));
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

	if (limitType == RESGROUP_LIMIT_TYPE_CONCURRENCY &&
		concurrency == 0 &&
		groupid == ADMINRESGROUP_OID)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_LIMIT_VALUE),
				 errmsg("admin_group must have at least one concurrency")));
	}

	/* Argument of callback function should be allocated in heap region */
	callbackCtx = (ResourceGroupAlterCallbackContext *)
		MemoryContextAlloc(TopMemoryContext, sizeof(*callbackCtx));
	callbackCtx->groupid = groupid;
	callbackCtx->limittype = limitType;

	/*
	 * In validateCapabilities() we scan all the resource groups
	 * to check whether the total cpu_rate_limit exceed 100 or not.
	 * We need to use AccessExclusiveLock here to prevent concurrent
	 * increase on different resource group.
	 */
	pg_resgroupcapability_rel = heap_open(ResGroupCapabilityRelationId,
										  AccessExclusiveLock);

	/* Load currency resource group capabilities */
	GetResGroupCapabilities(groupid, &caps);

	/* Pick up the effective settings from caps */
	ResGroupCapsToOpts(&caps, &opts);

	/* Attempt to pick previous 'proposed' as 'value' */
	ResGroupDecideConcurrencyCaps(groupid, &caps, &opts);
	ResGroupDecideMemoryCaps(groupid, &caps, &opts);

	switch (limitType)
	{
		case RESGROUP_LIMIT_TYPE_CONCURRENCY:
			opts.concurrency = concurrency;
			ResGroupDecideConcurrencyCaps(groupid, &caps, &opts);
			break;

		case RESGROUP_LIMIT_TYPE_CPU:
			opts.cpuRateLimit = cpuRateLimitNew;
			if (caps.cpuRateLimit.proposed < cpuRateLimitNew)
				validateCapabilities(pg_resgroupcapability_rel,
									 groupid, &opts, false);

			caps.cpuRateLimit.value = cpuRateLimitNew;
			caps.cpuRateLimit.proposed = cpuRateLimitNew;
			break;

		case RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA:
			opts.memSharedQuota = memSharedQuotaNew;

			ResGroupDecideMemoryCaps(groupid, &caps, &opts);
			break;

		case RESGROUP_LIMIT_TYPE_MEMORY:
			opts.memLimit = memLimitNew;
			if (caps.memLimit.proposed < memLimitNew)
				validateCapabilities(pg_resgroupcapability_rel,
									 groupid, &opts, false);

			ResGroupDecideMemoryCaps(groupid, &caps, &opts);
			break;
		case RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO:
			caps.memSpillRatio.value = memSpillRatioNew;
			caps.memSpillRatio.proposed = memSpillRatioNew;
			break;

		default:
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("unsupported resource group limit type '%s'", defel->defname)));
	}

	updateResgroupCapabilities(groupid, &caps);

	heap_close(pg_resgroupcapability_rel, NoLock);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR|
									DF_WITH_SNAPSHOT|
									DF_NEED_TWO_PHASE,
									GetAssignedOidsForDispatch(),
									NULL);
	}

	/* Bump command counter to make this change visible in the callback function alterResGroupCommitCallback() */
	CommandCounterIncrement();

	if (IsResGroupActivated())
	{
		callbackCtx->caps = caps;
		registerResourceGroupCallback(alterResGroupCallback, (void *)callbackCtx);
	}
}

/*
 * Get all the capabilities of one resource group in pg_resgroupcapability.
 */
void
GetResGroupCapabilities(Oid groupId, ResGroupCaps *resgroupCaps)
{
	SysScanDesc	sscan;
	ScanKeyData	key;
	HeapTuple	tuple;
	bool isNull;
	Relation	relResGroupCapability;
	/*
	 * By converting caps from (ResGroupCaps *) to an array of (ResGroupCap *)
	 * we can access the individual capability via index, so we don't need
	 * to use a switch case when setting them.
	 */
	ResGroupCap *caps = (ResGroupCap *) resgroupCaps;
	ResourceOwner owner = NULL;
	/*
	 * We maintain a bit mask to track which resgroup limit capability types
	 * have been retrieved, when mask is 0 then no limit capability is found
	 * for the given groupId.
	 */
	int			mask = 0;

	MemSet(caps, 0, sizeof(ResGroupCaps));

	if (CurrentResourceOwner == NULL)
	{
		owner = ResourceOwnerCreate(NULL, "getResgroupCapabilityEntry");
		CurrentResourceOwner = owner;
	}

	relResGroupCapability = heap_open(ResGroupCapabilityRelationId, AccessShareLock);

	ScanKeyInit(&key,
				Anum_pg_resgroupcapability_resgroupid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(groupId));

	sscan = systable_beginscan(relResGroupCapability,
							   ResGroupCapabilityResgroupidIndexId,
							   true,
							   SnapshotNow, 1, &key);

	while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
	{
		Datum				typeDatum;
		ResGroupLimitType	type;
		Datum				valueDatum;
		Datum				proposedDatum;
		char				*value;
		char				*proposed;

		typeDatum = heap_getattr(tuple, Anum_pg_resgroupcapability_reslimittype,
								 relResGroupCapability->rd_att, &isNull);
		type = (ResGroupLimitType) DatumGetInt16(typeDatum);

		Assert(type > RESGROUP_LIMIT_TYPE_UNKNOWN);
		Assert(type < RESGROUP_LIMIT_TYPE_COUNT);
		Assert(!(mask & (1 << type)));

		mask |= 1 << type;

		valueDatum = heap_getattr(tuple, Anum_pg_resgroupcapability_value, relResGroupCapability->rd_att, &isNull);
		value = TextDatumGetCString(valueDatum);
		caps[type].value = str2Int(value, getResgroupOptionName(type));

		proposedDatum = heap_getattr(tuple, Anum_pg_resgroupcapability_proposed, relResGroupCapability->rd_att, &isNull);
		proposed = TextDatumGetCString(proposedDatum);
		caps[type].proposed = str2Int(proposed, getResgroupOptionName(type));
	}

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

	if (!mask)
	{
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("Cannot find limit capabilities for resource group: %d",
						groupId)));
	}
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
 * Get the option type from a name string.
 *
 * @param defname  the name string
 *
 * @return the option type or UNKNOWN if the name is unknown
 */
static ResGroupLimitType
getResgroupOptionType(const char* defname)
{
	if (strcmp(defname, "cpu_rate_limit") == 0)
		return RESGROUP_LIMIT_TYPE_CPU;
	else if (strcmp(defname, "memory_limit") == 0)
		return RESGROUP_LIMIT_TYPE_MEMORY;
	else if (strcmp(defname, "concurrency") == 0)
		return RESGROUP_LIMIT_TYPE_CONCURRENCY;
	else if (strcmp(defname, "memory_shared_quota") == 0)
		return RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA;
	else if (strcmp(defname, "memory_spill_ratio") == 0)
		return RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO;
	else
		return RESGROUP_LIMIT_TYPE_UNKNOWN;
}

/*
 * Get the option name from type.
 *
 * @param type  the resgroup limit type
 *
 * @return the name of type
 */
static const char *
getResgroupOptionName(ResGroupLimitType type)
{
	switch (type)
	{
		case RESGROUP_LIMIT_TYPE_CONCURRENCY:
			return "concurrency";
		case RESGROUP_LIMIT_TYPE_CPU:
			return "cpu_rate_limit";
		case RESGROUP_LIMIT_TYPE_MEMORY:
			return "memory_limit";
		case RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA:
			return "memory_shared_quota";
		case RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO:
			return "memory_spill_ratio";
		default:
			return "unknown";
	}
}

/*
 * Parse a statement and store the settings in options.
 *
 * @param stmt     the statement
 * @param options  used to store the settings
 */
static void
parseStmtOptions(CreateResourceGroupStmt *stmt, ResGroupOpts *options)
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
				if (options->concurrency < RESGROUP_MIN_CONCURRENCY ||
					options->concurrency > RESGROUP_MAX_CONCURRENCY)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							errmsg("concurrency range is [%d, 'max_connections']",
								   RESGROUP_MIN_CONCURRENCY)));
				break;

			case RESGROUP_LIMIT_TYPE_CPU:
				options->cpuRateLimit = defGetInt64(defel);
				if (options->cpuRateLimit < RESGROUP_MIN_CPU_RATE_LIMIT ||
					options->cpuRateLimit > RESGROUP_MAX_CPU_RATE_LIMIT)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							errmsg("cpu_rate_limit range is [%d, %d]",
								   RESGROUP_MIN_CPU_RATE_LIMIT,
								   RESGROUP_MAX_CPU_RATE_LIMIT)));
				break;

			case RESGROUP_LIMIT_TYPE_MEMORY:
				options->memLimit = defGetInt64(defel);
				if (options->memLimit < RESGROUP_MIN_MEMORY_LIMIT ||
					options->memLimit > RESGROUP_MAX_MEMORY_LIMIT)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							errmsg("memory_limit range is [%d, %d]",
								   RESGROUP_MIN_MEMORY_LIMIT,
								   RESGROUP_MAX_MEMORY_LIMIT)));
				break;

			case RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA:
				options->memSharedQuota = defGetInt64(defel);
				if (options->memSharedQuota < RESGROUP_MIN_MEMORY_SHARED_QUOTA ||
					options->memSharedQuota > RESGROUP_MAX_MEMORY_SHARED_QUOTA)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							errmsg("memory_shared_quota range is [%d, %d]",
								   RESGROUP_MIN_MEMORY_SHARED_QUOTA,
								   RESGROUP_MAX_MEMORY_SHARED_QUOTA)));
				break;

			case RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO:
				options->memSpillRatio = defGetInt64(defel);
				if (options->memSpillRatio < RESGROUP_MIN_MEMORY_SPILL_RATIO ||
					options->memSpillRatio > RESGROUP_MAX_MEMORY_SPILL_RATIO)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							errmsg("memory_spill_ratio range is [%d, %d]",
								   RESGROUP_MIN_MEMORY_SPILL_RATIO,
								   RESGROUP_MAX_MEMORY_SPILL_RATIO)));
				break;

			default:
				Assert(!"unexpected options");
				break;
		}
	}

	if (options->memLimit == 0 || options->cpuRateLimit == 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("must specify both memory_limit and cpu_rate_limit")));

	if (!(types & (1 << RESGROUP_LIMIT_TYPE_CONCURRENCY)))
		options->concurrency = RESGROUP_DEFAULT_CONCURRENCY;

	if (!(types & (1 << RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA)))
		options->memSharedQuota = RESGROUP_DEFAULT_MEM_SHARED_QUOTA;

	if (!(types & (1 << RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO)))
		options->memSpillRatio = RESGROUP_DEFAULT_MEM_SPILL_RATIO;
}

/*
 * Resource group call back function
 *
 * Remove resource group entry in shared memory when abort transaction which
 * creates resource groups
 */
static void
createResGroupCallback(bool isCommit, void *arg)
{
	Oid groupId;

	groupId = *(Oid *)arg;
	pfree(arg);

	if (isCommit)
		return;

	ResGroupCreateOnAbort(groupId);
}

/*
 * Resource group call back function
 *
 * When DROP RESOURCE GROUP transaction ends, need wake up
 * the queued transactions and cleanup shared menory entry.
 */
static void
dropResGroupCallback(bool isCommit, void *arg)
{
	Oid groupId;

	groupId = *(Oid *)arg;
	pfree(arg);

	ResGroupDropFinish(groupId, isCommit);
}

/*
 * Resource group call back function
 *
 * When ALTER RESOURCE GROUP SET CONCURRENCY commits, some queuing
 * transaction of this resource group may need to be woke up.
 */
static void
alterResGroupCallback(bool isCommit, void *arg)
{
	ResourceGroupAlterCallbackContext *ctx =
		(ResourceGroupAlterCallbackContext *) arg;

	if (isCommit)
		ResGroupAlterOnCommit(ctx->groupid, ctx->limittype, &ctx->caps);

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
						   ResGroupOpts *options)
{
	char value[64];
	Relation resgroup_capability_rel = heap_open(ResGroupCapabilityRelationId, RowExclusiveLock);

	validateCapabilities(resgroup_capability_rel, groupid, options, true);

	sprintf(value, "%d", options->concurrency);
	insertResgroupCapabilityEntry(resgroup_capability_rel, groupid, RESGROUP_LIMIT_TYPE_CONCURRENCY, value);

	sprintf(value, "%d", options->cpuRateLimit);
	insertResgroupCapabilityEntry(resgroup_capability_rel, groupid, RESGROUP_LIMIT_TYPE_CPU, value);

	sprintf(value, "%d", options->memLimit);
	insertResgroupCapabilityEntry(resgroup_capability_rel, groupid, RESGROUP_LIMIT_TYPE_MEMORY, value);

	sprintf(value, "%d", options->memSharedQuota);
	insertResgroupCapabilityEntry(resgroup_capability_rel, groupid, RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA, value);

	sprintf(value, "%d", options->memSpillRatio);
	insertResgroupCapabilityEntry(resgroup_capability_rel, groupid, RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO, value);

	heap_close(resgroup_capability_rel, NoLock);
}

/*
 * Update all the capabilities of one resgroup in pg_resgroupcapability
 *
 * groupid is the update key, resgroupCaps contains all the capabilities.
 */
static void
updateResgroupCapabilities(Oid groupid, const ResGroupCaps *resgroupCaps)
{
	HeapTuple	oldTuple;
	HeapTuple	newTuple;
	SysScanDesc	sscan;
	ScanKeyData	scankey;
	Datum		values[Natts_pg_resgroupcapability];
	bool		isnull[Natts_pg_resgroupcapability];
	bool		repl[Natts_pg_resgroupcapability];
	/*
	 * By converting caps from (ResGroupCaps *) to an array of (ResGroupCap *)
	 * we can access the individual capability via index, so we don't need
	 * to use a switch case when setting them.
	 */
	const ResGroupCap	*caps = (ResGroupCap *) resgroupCaps;
	/*
	 * We maintain a bit mask to track which resgroup limit capability types
	 * have been retrieved, when mask is 0 then no limit capability is found
	 * for the given groupid.
	 */
	int			mask = 0;

	Relation resgroupCapabilityRel = heap_open(ResGroupCapabilityRelationId, RowExclusiveLock);

	ScanKeyInit(&scankey,
				Anum_pg_resgroupcapability_resgroupid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(groupid));

	sscan = systable_beginscan(resgroupCapabilityRel, ResGroupCapabilityResgroupidIndexId, true,
							   SnapshotNow, 1, &scankey);

	MemSet(values, 0, sizeof(values));
	MemSet(isnull, 0, sizeof(isnull));
	MemSet(repl, 0, sizeof(repl));

	while (HeapTupleIsValid(oldTuple = systable_getnext(sscan)))
	{
		Datum				typeDatum;
		ResGroupLimitType	type;
		char				value[16];
		char				proposed[16];

		typeDatum = heap_getattr(oldTuple, Anum_pg_resgroupcapability_reslimittype,
								 resgroupCapabilityRel->rd_att, isnull);
		type = (ResGroupLimitType) DatumGetInt16(typeDatum);

		Assert(type > RESGROUP_LIMIT_TYPE_UNKNOWN);
		Assert(type < RESGROUP_LIMIT_TYPE_COUNT);
		Assert(!(mask & (1 << type)));

		mask |= 1 << type;

		snprintf(value, sizeof(value), "%d", caps[type].value);
		snprintf(proposed, sizeof(proposed), "%d", caps[type].proposed);

		values[Anum_pg_resgroupcapability_value - 1] = CStringGetTextDatum(value);
		isnull[Anum_pg_resgroupcapability_value - 1] = false;
		repl[Anum_pg_resgroupcapability_value - 1]  = true;

		values[Anum_pg_resgroupcapability_proposed - 1] = CStringGetTextDatum(proposed);
		isnull[Anum_pg_resgroupcapability_proposed - 1] = false;
		repl[Anum_pg_resgroupcapability_proposed - 1]  = true;

		newTuple = heap_modify_tuple(oldTuple, RelationGetDescr(resgroupCapabilityRel),
									 values, isnull, repl);

		simple_heap_update(resgroupCapabilityRel, &oldTuple->t_self, newTuple);
		CatalogUpdateIndexes(resgroupCapabilityRel, newTuple);
	}

	systable_endscan(sscan);
	heap_close(resgroupCapabilityRel, NoLock);

	if (!mask)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("capabilities missing for resource group: %d", groupid)));
	}
}

/*
 * Validate the capabilities.
 *
 * The policy is resouces can't be over used, take memory for example,
 * all the allocated memory can not exceed 100.
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
					 ResGroupOpts *options,
					 bool newGroup)
{
	HeapTuple tuple;
	SysScanDesc sscan;
	int totalCpu = options->cpuRateLimit;
	int totalMem = options->memLimit;

	sscan = systable_beginscan(rel, ResGroupCapabilityResgroupidIndexId, true, SnapshotNow, 0, NULL);

	while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
	{
		Datum				groupIdDatum;
		Datum				typeDatum;
		Datum				proposedDatum;
		ResGroupLimitType	reslimittype;
		Oid					resgroupid;
		char				*proposedStr;
		int					proposed;
		bool				isNull;

		groupIdDatum = heap_getattr(tuple, Anum_pg_resgroupcapability_resgroupid,
									rel->rd_att, &isNull);
		resgroupid = DatumGetObjectId(groupIdDatum);

		if (resgroupid == groupid)
		{
			if (!newGroup)
				continue;

			ereport(ERROR,
					(errcode(ERRCODE_DUPLICATE_OBJECT),
					errmsg("Find duplicate resoure group id:%d", groupid)));
		}

		typeDatum = heap_getattr(tuple, Anum_pg_resgroupcapability_reslimittype,
								 rel->rd_att, &isNull);
		reslimittype = (ResGroupLimitType) DatumGetInt16(typeDatum);

		proposedDatum = heap_getattr(tuple, Anum_pg_resgroupcapability_proposed,
									 rel->rd_att, &isNull);
		proposedStr = TextDatumGetCString(proposedDatum);
		proposed = str2Int(proposedStr, getResgroupOptionName(reslimittype));

		if (reslimittype == RESGROUP_LIMIT_TYPE_CPU)
		{
			totalCpu += proposed;
			if (totalCpu > RESGROUP_MAX_CPU_RATE_LIMIT)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						errmsg("total cpu_rate_limit exceeded the limit of %d",
							   RESGROUP_MAX_CPU_RATE_LIMIT)));
		}
		else if (reslimittype == RESGROUP_LIMIT_TYPE_MEMORY)
		{
			totalMem += proposed;
			if (totalMem > RESGROUP_MAX_MEMORY_LIMIT)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						errmsg("total memory_limit exceeded the limit of %d",
							   RESGROUP_MAX_MEMORY_LIMIT)));
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
 * Convert a C str to a integer value.
 *
 * @param str   the C str
 * @param prop  the property name
 */
static int
str2Int(const char *str, const char *prop)
{
	char *end = NULL;
	double val = strtod(str, &end);

	/* both the property name and value are already checked
	 * by the syntax parser, but we'll check it again anyway for safe. */
	if (end == NULL || end == str || *end != 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("%s requires a numeric value", prop)));

	return floor(val);
}


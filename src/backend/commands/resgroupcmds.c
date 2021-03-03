/*-------------------------------------------------------------------------
 *
 * resgroupcmds.c
 *	  Commands for manipulating resource group.
 *
 * Portions Copyright (c) 2006-2017, Greenplum inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 * IDENTIFICATION
 *    src/backend/commands/resgroupcmds.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "funcapi.h"
#include "libpq-fe.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/xact.h"
#include "catalog/heap.h"
#include "catalog/oid_dispatch.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_resgroup.h"
#include "catalog/pg_resgroupcapability.h"
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
#include "utils/faultinjector.h"

#define RESGROUP_DEFAULT_CONCURRENCY (20)
#define RESGROUP_DEFAULT_MEM_SHARED_QUOTA (80)
#define RESGROUP_DEFAULT_MEM_SPILL_RATIO RESGROUP_FALLBACK_MEMORY_SPILL_RATIO
#define RESGROUP_DEFAULT_MEMORY_LIMIT RESGROUP_UNLIMITED_MEMORY_LIMIT

#define RESGROUP_DEFAULT_MEM_AUDITOR (RESGROUP_MEMORY_AUDITOR_VMTRACKER)
#define RESGROUP_INVALID_MEM_AUDITOR (-1)

#define RESGROUP_MIN_CONCURRENCY	(0)
#define RESGROUP_MAX_CONCURRENCY	(MaxConnections)

#define RESGROUP_MIN_CPU_RATE_LIMIT	(1)
#define RESGROUP_MAX_CPU_RATE_LIMIT	(100)

#define RESGROUP_MIN_MEMORY_LIMIT	(0)
#define RESGROUP_MAX_MEMORY_LIMIT	(100)

#define RESGROUP_MIN_MEMORY_SHARED_QUOTA	(0)
#define RESGROUP_MAX_MEMORY_SHARED_QUOTA	(100)

#define RESGROUP_MIN_MEMORY_SPILL_RATIO		(0)
#define RESGROUP_MAX_MEMORY_SPILL_RATIO		(100)

/*
 * The names must be in the same order as ResGroupMemAuditorType.
 */
static const char *ResGroupMemAuditorName[] =
{
	"vmtracker",	// RESGROUP_MEMORY_AUDITOR_VMTRACKER
	"cgroup"		// RESGROUP_MEMORY_AUDITOR_CGROUP
};

static int str2Int(const char *str, const char *prop);
static ResGroupLimitType getResgroupOptionType(const char* defname);
static ResGroupCap getResgroupOptionValue(DefElem *defel, int type);
static const char *getResgroupOptionName(ResGroupLimitType type);
static void checkResgroupCapLimit(ResGroupLimitType type, ResGroupCap value);
static void checkResgroupCapConflicts(ResGroupCaps *caps);
static void parseStmtOptions(CreateResourceGroupStmt *stmt, ResGroupCaps *caps);
static void validateCapabilities(Relation rel, Oid groupid, ResGroupCaps *caps, bool newGroup);
static void insertResgroupCapabilityEntry(Relation rel, Oid groupid, uint16 type, const char *value);
static void updateResgroupCapabilityEntry(Relation rel,
										  Oid groupId,
										  ResGroupLimitType limitType,
										  ResGroupCap value,
										  const char *strValue);
static void insertResgroupCapabilities(Relation rel, Oid groupId, ResGroupCaps *caps);
static void deleteResgroupCapabilities(Oid groupid);
static void checkAuthIdForDrop(Oid groupId);
static void createResgroupCallback(XactEvent event, void *arg);
static void dropResgroupCallback(XactEvent event, void *arg);
static void alterResgroupCallback(XactEvent event, void *arg);
static int getResGroupMemAuditor(char *name);
static bool checkCpusetSyntax(const char *cpuset);

/*
 * CREATE RESOURCE GROUP
 */
void
CreateResourceGroup(CreateResourceGroupStmt *stmt)
{
	Relation	pg_resgroup_rel;
	Relation	pg_resgroupcapability_rel;
	TupleDesc	pg_resgroup_dsc;
	ScanKeyData scankey;
	SysScanDesc sscan;
	HeapTuple	tuple;
	Oid			groupid;
	Datum		new_record[Natts_pg_resgroup];
	bool		new_record_nulls[Natts_pg_resgroup];
	ResGroupCaps caps;
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

	ClearResGroupCaps(&caps);
	parseStmtOptions(stmt, &caps);

	/*
	 * both CREATE and ALTER resource group need check the sum of cpu_rate_limit
	 * and memory_limit and make sure the sum don't exceed 100. To make it simple,
	 * acquire AccessExclusiveLock lock on pg_resgroupcapability at the beginning
	 * of CREATE and ALTER
	 */
	pg_resgroupcapability_rel = table_open(ResGroupCapabilityRelationId, AccessExclusiveLock);
	pg_resgroup_rel = table_open(ResGroupRelationId, RowExclusiveLock);

	/* Check if MaxResourceGroups limit is reached */
	sscan = systable_beginscan(pg_resgroup_rel, ResGroupRsgnameIndexId, false,
							   NULL, 0, NULL);
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
							   NULL, 1, &scankey);

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

	new_record[Anum_pg_resgroup_parent - 1] = ObjectIdGetDatum(0);

	groupid = GetNewOidForResGroup(pg_resgroup_rel, ResGroupOidIndexId,
								   Anum_pg_resgroup_oid,
								   stmt->name);
	new_record[Anum_pg_resgroup_oid - 1] = groupid;

	pg_resgroup_dsc = RelationGetDescr(pg_resgroup_rel);
	tuple = heap_form_tuple(pg_resgroup_dsc, new_record, new_record_nulls);

	/*
	 * Insert new record in the pg_resgroup table
	 */
	CatalogTupleInsert(pg_resgroup_rel, tuple);

	/* process the WITH (...) list items */
	validateCapabilities(pg_resgroupcapability_rel, groupid, &caps, true);
	insertResgroupCapabilities(pg_resgroupcapability_rel, groupid, &caps);

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

	table_close(pg_resgroup_rel, NoLock);
	table_close(pg_resgroupcapability_rel, NoLock);

	/* Add this group into shared memory */
	if (IsResGroupActivated())
	{
		ResourceGroupCallbackContext *callbackCtx;

		AllocResGroupEntry(groupid, &caps);

		/* Argument of callback function should be allocated in heap region */
		callbackCtx = (ResourceGroupCallbackContext *)
			MemoryContextAlloc(TopMemoryContext, sizeof(*callbackCtx));
		callbackCtx->groupid = groupid;
		callbackCtx->caps = caps;
		RegisterXactCallbackOnce(createResgroupCallback, callbackCtx);

		/* Create os dependent part for this resource group */
		ResGroupOps_CreateGroup(groupid);

		ResGroupOps_SetMemoryLimit(groupid, caps.memLimit);

		if (caps.cpuRateLimit != CPU_RATE_LIMIT_DISABLED)
		{
			ResGroupOps_SetCpuRateLimit(groupid, caps.cpuRateLimit);
		}
		else if (!CpusetIsEmpty(caps.cpuset))
		{
			EnsureCpusetIsAvailable(ERROR);

			ResGroupOps_SetCpuSet(groupid, caps.cpuset);
			/* reset default group, subtract new group cpu cores */
			char defaultGroupCpuset[MaxCpuSetLength];
			ResGroupOps_GetCpuSet(DEFAULT_CPUSET_GROUP_ID,
								  defaultGroupCpuset,
								  MaxCpuSetLength);
			CpusetDifference(defaultGroupCpuset, caps.cpuset, MaxCpuSetLength);
			ResGroupOps_SetCpuSet(DEFAULT_CPUSET_GROUP_ID, defaultGroupCpuset);
		}
		SIMPLE_FAULT_INJECTOR("create_resource_group_fail");
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
	HeapTuple	 tuple;
	ScanKeyData	 scankey;
	SysScanDesc	 sscan;
	Oid			 groupid;
	ResourceGroupCallbackContext	*callbackCtx;

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
							   NULL, 1, &scankey);

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
	groupid = ((Form_pg_resgroup) GETSTRUCT(tuple))->oid;

	/* cannot DROP default resource groups  */
	if (groupid == DEFAULTRESGROUP_OID || groupid == ADMINRESGROUP_OID)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot drop default resource group \"%s\"",
						stmt->name)));

	/* check before dispatch to segment */
	if (IsResGroupActivated())
	{
		/* Argument of callback function should be allocated in heap region */
		callbackCtx = (ResourceGroupCallbackContext *)
			MemoryContextAlloc(TopMemoryContext, sizeof(*callbackCtx));
		callbackCtx->groupid = groupid;
		RegisterXactCallbackOnce(dropResgroupCallback, callbackCtx);

		ResGroupCheckForDrop(groupid, stmt->name);
	}

	/*
	 * Check to see if any roles are in this resource group.
	 */
	checkAuthIdForDrop(groupid);

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

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		/* metadata tracking */
		MetaTrackDropObject(ResGroupRelationId, groupid);

		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR|
									DF_WITH_SNAPSHOT|
									DF_NEED_TWO_PHASE,
									NIL,
									NULL);
	}
}

/*
 * ALTER RESOURCE GROUP
 */
void
AlterResourceGroup(AlterResourceGroupStmt *stmt)
{
	Relation	pg_resgroupcapability_rel;
	Oid			groupid;
	DefElem		*defel;
	ResGroupLimitType	limitType;
	ResGroupCaps		caps;
	ResGroupCaps		oldCaps;
	ResGroupCap			value = 0;
	const char *cpuset = NULL;
	ResourceGroupCallbackContext	*callbackCtx;

	/* Permission check - only superuser can alter resource groups. */
	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("must be superuser to alter resource groups")));

	/* Currently we only support to ALTER one limit at one time */
	Assert(list_length(stmt->options) == 1);
	defel = (DefElem *) lfirst(list_head(stmt->options));

	limitType = getResgroupOptionType(defel->defname);
	if (limitType == RESGROUP_LIMIT_TYPE_UNKNOWN)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("option \"%s\" not recognized", defel->defname)));
	}
	else if (limitType == RESGROUP_LIMIT_TYPE_CPUSET)
	{
		EnsureCpusetIsAvailable(ERROR);

		cpuset = defGetString(defel);
		checkCpusetSyntax(cpuset);
	}
	else
	{
		value = getResgroupOptionValue(defel, limitType);
		checkResgroupCapLimit(limitType, value);
	}

	/*
	 * Check the pg_resgroup relation to be certain the resource group already
	 * exists.
	 */
	groupid = get_resgroup_oid(stmt->name, false);

	if (limitType == RESGROUP_LIMIT_TYPE_CONCURRENCY &&
		value == 0 &&
		groupid == ADMINRESGROUP_OID)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_LIMIT_VALUE),
				 errmsg("admin_group must have at least one concurrency")));
	}

	/*
	 * In validateCapabilities() we scan all the resource groups
	 * to check whether the total cpu_rate_limit exceed 100 or not.
	 * We need to use AccessExclusiveLock here to prevent concurrent
	 * increase on different resource group.
	 */
	pg_resgroupcapability_rel = heap_open(ResGroupCapabilityRelationId,
										  AccessExclusiveLock);

	/* Load current resource group capabilities */
	GetResGroupCapabilities(pg_resgroupcapability_rel, groupid, &oldCaps);
	caps = oldCaps;

	switch (limitType)
	{
		case RESGROUP_LIMIT_TYPE_CPU:
			caps.cpuRateLimit = value;
			SetCpusetEmpty(caps.cpuset, sizeof(caps.cpuset));
			break;
		case RESGROUP_LIMIT_TYPE_MEMORY:
			caps.memLimit = value;
			break;
		case RESGROUP_LIMIT_TYPE_CONCURRENCY:
			caps.concurrency = value;
			break;
		case RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA:
			caps.memSharedQuota = value;
			break;
		case RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO:
			caps.memSpillRatio = value;
			break;
		case RESGROUP_LIMIT_TYPE_MEMORY_AUDITOR:
			caps.memAuditor = value;
			break;
		case RESGROUP_LIMIT_TYPE_CPUSET:
			StrNCpy(caps.cpuset, cpuset, sizeof(caps.cpuset));
			caps.cpuRateLimit = CPU_RATE_LIMIT_DISABLED;
			break;
		default:
			break;
	}

	checkResgroupCapConflicts(&caps);

	validateCapabilities(pg_resgroupcapability_rel, groupid, &caps, false);

	/* cpuset & cpu_rate_limit can not coexist 
	 * if cpuset is active, then cpu_rate_limit must set to CPU_RATE_LIMIT_DISABLED
	 * if cpu_rate_limit is active, then cpuset must set to "" */
	if (limitType == RESGROUP_LIMIT_TYPE_CPUSET)
	{
		updateResgroupCapabilityEntry(pg_resgroupcapability_rel,
									  groupid, RESGROUP_LIMIT_TYPE_CPU, 
									  CPU_RATE_LIMIT_DISABLED, "");
		updateResgroupCapabilityEntry(pg_resgroupcapability_rel,
									  groupid, RESGROUP_LIMIT_TYPE_CPUSET, 
									  0, caps.cpuset);
	}
	else if (limitType == RESGROUP_LIMIT_TYPE_CPU)
	{
		updateResgroupCapabilityEntry(pg_resgroupcapability_rel,
									  groupid, RESGROUP_LIMIT_TYPE_CPUSET,
									  0, DefaultCpuset);
		updateResgroupCapabilityEntry(pg_resgroupcapability_rel,
									  groupid, RESGROUP_LIMIT_TYPE_CPU,
									  value, "");
	}
	else
	{
		updateResgroupCapabilityEntry(pg_resgroupcapability_rel,
									  groupid, limitType, value, "");
	}

	heap_close(pg_resgroupcapability_rel, NoLock);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		MetaTrackUpdObject(ResGroupCapabilityRelationId,
						   groupid,
						   GetUserId(),
						   "ALTER", defel->defname);

		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR|
									DF_WITH_SNAPSHOT|
									DF_NEED_TWO_PHASE,
									GetAssignedOidsForDispatch(),
									NULL);
	}

	if (IsResGroupActivated())
	{
		/* Argument of callback function should be allocated in heap region */
		callbackCtx = (ResourceGroupCallbackContext *)
			MemoryContextAlloc(TopMemoryContext, sizeof(*callbackCtx));
		callbackCtx->groupid = groupid;
		callbackCtx->limittype = limitType;
		callbackCtx->caps = caps;
		callbackCtx->oldCaps = oldCaps;
		RegisterXactCallbackOnce(alterResgroupCallback, callbackCtx);
	}
}

/*
 * Get all the capabilities of one resource group in pg_resgroupcapability.
 */
void
GetResGroupCapabilities(Relation rel, Oid groupId, ResGroupCaps *resgroupCaps)
{
	SysScanDesc	sscan;
	ScanKeyData	key;
	HeapTuple	tuple;
	bool isNull;

	/*
	 * We maintain a bit mask to track which resgroup limit capability types
	 * have been retrieved, when mask is 0 then no limit capability is found
	 * for the given groupId.
	 */
	int			mask = 0;

	ClearResGroupCaps(resgroupCaps);

	/* Init cpuset with proper value */
	strcpy(resgroupCaps->cpuset, DefaultCpuset);

	ScanKeyInit(&key,
				Anum_pg_resgroupcapability_resgroupid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(groupId));

	sscan = systable_beginscan(rel,
							   ResGroupCapabilityResgroupidIndexId,
							   true,
							   NULL, 1, &key);

	while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
	{
		Datum				typeDatum;
		ResGroupLimitType	type;
		Datum				valueDatum;
		char				*value;

		typeDatum = heap_getattr(tuple, Anum_pg_resgroupcapability_reslimittype,
								 rel->rd_att, &isNull);
		type = (ResGroupLimitType) DatumGetInt16(typeDatum);

		Assert(type > RESGROUP_LIMIT_TYPE_UNKNOWN);
		Assert(!(mask & (1 << type)));

		if (type >= RESGROUP_LIMIT_TYPE_COUNT)
			continue;

		mask |= 1 << type;

		valueDatum = heap_getattr(tuple, Anum_pg_resgroupcapability_value,
									 rel->rd_att, &isNull);
		value = TextDatumGetCString(valueDatum);
		switch (type)
		{
			case RESGROUP_LIMIT_TYPE_CONCURRENCY:
				resgroupCaps->concurrency = str2Int(value,
													getResgroupOptionName(type));
				break;
			case RESGROUP_LIMIT_TYPE_CPU:
				resgroupCaps->cpuRateLimit = str2Int(value,
													 getResgroupOptionName(type));
				break;
			case RESGROUP_LIMIT_TYPE_MEMORY:
				resgroupCaps->memLimit = str2Int(value,
												 getResgroupOptionName(type));
				break;
			case RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA:
				resgroupCaps->memSharedQuota = str2Int(value,
													   getResgroupOptionName(type));
				break;
			case RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO:
				resgroupCaps->memSpillRatio = str2Int(value,
													  getResgroupOptionName(type));
				break;
			case RESGROUP_LIMIT_TYPE_MEMORY_AUDITOR:
				resgroupCaps->memAuditor = str2Int(value,
												   getResgroupOptionName(type));
				break;
			case RESGROUP_LIMIT_TYPE_CPUSET:
				StrNCpy(resgroupCaps->cpuset, value, sizeof(resgroupCaps->cpuset));
				break;
			default:
				break;
		}
	}

	systable_endscan(sscan);

	if (!mask)
	{
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("cannot find limit capabilities for resource group: %d",
						groupId)));
	}
}

/*
 * Get resource group id for a role in pg_authid.
 *
 * An exception is thrown if the current role is invalid. This can happen if,
 * for example, a role was logged into psql and that role was dropped by another
 * psql session. But, normally something like "ERROR:  role 16385 was
 * concurrently dropped" would happen before the code reaches this function.
 */
Oid
GetResGroupIdForRole(Oid roleid)
{
	HeapTuple	tuple;
	Oid			groupId;
	bool		isNull;
	Relation	rel;
	ScanKeyData	key;
	SysScanDesc	 sscan;

	rel = table_open(AuthIdRelationId, AccessShareLock);

	ScanKeyInit(&key,
				Anum_pg_authid_oid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(roleid));

	sscan = systable_beginscan(rel, AuthIdOidIndexId, true, NULL, 1, &key);

	tuple = systable_getnext(sscan);
	if (!HeapTupleIsValid(tuple))
	{
		systable_endscan(sscan);
		table_close(rel, AccessShareLock);

		/*
		 * Role should have been dropped by other backends in this case, so this
		 * session cannot execute any command anymore
		 */
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				errmsg("role with Oid %d was dropped", roleid),
				errdetail("Cannot execute commands anymore, please terminate this session.")));
	}

	/* must access tuple before systable_endscan */
	groupId = DatumGetObjectId(heap_getattr(tuple, Anum_pg_authid_rolresgroup,
							   rel->rd_att, &isNull));

	systable_endscan(sscan);

	/*
	 * release lock here to guarantee we have no lock held when acquiring
	 * resource group slot
	 */
	table_close(rel, AccessShareLock);

	if (!OidIsValid(groupId))
		groupId = InvalidOid;

	return groupId;
}

/*
 * get_resgroup_oid -- Return the Oid for a resource group name
 *
 * If missing_ok is false, throw an error if database name not found.  If
 * true, just return InvalidOid.
 *
 * Notes:
 *	Used by the various admin commands to convert a user supplied group name
 *	to Oid.
 */
Oid
get_resgroup_oid(const char *name, bool missing_ok)
{
	Oid			oid;

	oid = GetSysCacheOid1(RESGROUPNAME, Anum_pg_resgroup_oid,
						  CStringGetDatum(name));

	if (!OidIsValid(oid) && !missing_ok)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("resource group \"%s\" does not exist",
						name)));

	return oid;
}

/*
 * GetResGroupNameForId -- Return the resource group name for an Oid
 */
char *
GetResGroupNameForId(Oid oid)
{
	HeapTuple	tuple;
	char		*name;

	tuple = SearchSysCache1(RESGROUPOID,
							ObjectIdGetDatum(oid));
	if (HeapTupleIsValid(tuple))
	{
		bool		isnull;
		Datum		nameDatum;
		Name		resGroupName;

		nameDatum = SysCacheGetAttr(RESGROUPOID, tuple,
									Anum_pg_resgroup_rsgname,
									&isnull);
		Assert(!isnull);
		resGroupName = DatumGetName(nameDatum);
		name = pstrdup(NameStr(*resGroupName));
	}
	else
		return "unknown";

	ReleaseSysCache(tuple);

	return name;
}

/*
 * Check to see if the group can be assigned with role
 */
void
ResGroupCheckForRole(Oid groupId)
{
	Relation pg_resgroupcapability_rel;
	ResGroupCaps caps;

	pg_resgroupcapability_rel = heap_open(ResGroupCapabilityRelationId,
										  AccessShareLock);

	/* Load current resource group capabilities */
	GetResGroupCapabilities(pg_resgroupcapability_rel, groupId, &caps);
	if (caps.memAuditor == RESGROUP_MEMORY_AUDITOR_CGROUP)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("you cannot assign a role to this resource group"),
				 errdetail("The memory_auditor property for this group is not the default.")));

	heap_close(pg_resgroupcapability_rel, AccessShareLock);
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
	else if (strcmp(defname, "memory_auditor") == 0)
		return RESGROUP_LIMIT_TYPE_MEMORY_AUDITOR;
	else if (strcmp(defname, "cpuset") == 0)
		return RESGROUP_LIMIT_TYPE_CPUSET;
	else
		return RESGROUP_LIMIT_TYPE_UNKNOWN;
}

/*
 * Get capability value from DefElem, convert from int64 to int
 */
static ResGroupCap
getResgroupOptionValue(DefElem *defel, int type)
{
	int64 value;

	if (type == RESGROUP_LIMIT_TYPE_MEMORY_AUDITOR)
	{
		char *auditor_name = defGetString(defel);
		value = getResGroupMemAuditor(auditor_name);
	}
	else
	{
		value = defGetInt64(defel);
	}

	if (value < INT_MIN || value > INT_MAX)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("capability %s is out of range", defel->defname)));
	return (ResGroupCap)value;
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
		case RESGROUP_LIMIT_TYPE_MEMORY_AUDITOR:
			return "memory_auditor";
		case RESGROUP_LIMIT_TYPE_CPUSET:
			return "cpuset";
		default:
			return "unknown";
	}
}

/*
 * Check if capability value exceeds max and min value
 */
static void
checkResgroupCapLimit(ResGroupLimitType type, int value)
{
		switch (type)
		{
			case RESGROUP_LIMIT_TYPE_CONCURRENCY:
				if (value < RESGROUP_MIN_CONCURRENCY ||
					value > RESGROUP_MAX_CONCURRENCY)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							errmsg("concurrency range is [%d, 'max_connections']",
								   RESGROUP_MIN_CONCURRENCY)));
				break;

			case RESGROUP_LIMIT_TYPE_CPU:
				if (value < RESGROUP_MIN_CPU_RATE_LIMIT ||
					value > RESGROUP_MAX_CPU_RATE_LIMIT)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							errmsg("cpu_rate_limit range is [%d, %d]",
								   RESGROUP_MIN_CPU_RATE_LIMIT,
								   RESGROUP_MAX_CPU_RATE_LIMIT)));
				break;

			case RESGROUP_LIMIT_TYPE_MEMORY:
				if (value < RESGROUP_MIN_MEMORY_LIMIT ||
					value > RESGROUP_MAX_MEMORY_LIMIT)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							errmsg("memory_limit range is [%d, %d]",
								   RESGROUP_MIN_MEMORY_LIMIT,
								   RESGROUP_MAX_MEMORY_LIMIT)));
				break;

			case RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA:
				if (value < RESGROUP_MIN_MEMORY_SHARED_QUOTA ||
					value > RESGROUP_MAX_MEMORY_SHARED_QUOTA)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							errmsg("memory_shared_quota range is [%d, %d]",
								   RESGROUP_MIN_MEMORY_SHARED_QUOTA,
								   RESGROUP_MAX_MEMORY_SHARED_QUOTA)));
				break;

			case RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO:
				if (value < RESGROUP_MIN_MEMORY_SPILL_RATIO ||
					value > RESGROUP_MAX_MEMORY_SPILL_RATIO)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							errmsg("memory_spill_ratio range is [%d, %d]",
								   RESGROUP_MIN_MEMORY_SPILL_RATIO,
								   RESGROUP_MAX_MEMORY_SPILL_RATIO)));
				break;

			case RESGROUP_LIMIT_TYPE_MEMORY_AUDITOR:
				if (value != RESGROUP_MEMORY_AUDITOR_VMTRACKER &&
					value != RESGROUP_MEMORY_AUDITOR_CGROUP)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							errmsg("memory_auditor should be \"%s\" or \"%s\"",
								   ResGroupMemAuditorName[RESGROUP_MEMORY_AUDITOR_VMTRACKER],
								   ResGroupMemAuditorName[RESGROUP_MEMORY_AUDITOR_CGROUP])));
				break;

			default:
				Assert(!"unexpected options");
				break;
		}
}

/*
 * Check conflict settings in caps.
 */
static void
checkResgroupCapConflicts(ResGroupCaps *caps)
{
	/*
	 * When memory_limit is unlimited the memory_spill_ratio must be set to
	 * 'fallback' mode to use the statement_mem.
	 */
	if (caps->memLimit == RESGROUP_UNLIMITED_MEMORY_LIMIT &&
		caps->memSpillRatio != RESGROUP_FALLBACK_MEMORY_SPILL_RATIO)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("when memory_limit is unlimited memory_spill_ratio must be set to %d",
						RESGROUP_FALLBACK_MEMORY_SPILL_RATIO)));

	/*
	 * When memory_auditor is cgroup the concurrency must be 0.
	 */
	if (caps->memAuditor == RESGROUP_MEMORY_AUDITOR_CGROUP &&
		caps->concurrency != 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("resource group concurrency must be 0 when group memory_auditor is %s",
					ResGroupMemAuditorName[RESGROUP_MEMORY_AUDITOR_CGROUP])));

	/*
	 * The cgroup memory_auditor should not be used without a properly
	 * configured cgroup memory directory.
	 */
	if (caps->memAuditor == RESGROUP_MEMORY_AUDITOR_CGROUP &&
		!gp_resource_group_enable_cgroup_memory)
	{
		ereport(ERROR,
				(errcode(ERRCODE_GP_FEATURE_NOT_CONFIGURED),
				 errmsg("cgroup is not properly configured for the 'cgroup' memory auditor"),
				 errhint("Extra cgroup configurations are required to enable this feature, "
						 "please refer to the Greenplum Documentation for details")));
	}
}

/*
 * Parse a statement and store the settings in options.
 *
 * @param stmt     the statement
 * @param caps     used to store the settings
 */
static void
parseStmtOptions(CreateResourceGroupStmt *stmt, ResGroupCaps *caps)
{
	ListCell *cell;
	ResGroupCap value;
	int mask = 0;

	foreach(cell, stmt->options)
	{
		DefElem *defel = (DefElem *) lfirst(cell);
		int type = getResgroupOptionType(defel->defname);

		if (type == RESGROUP_LIMIT_TYPE_UNKNOWN)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("option \"%s\" not recognized", defel->defname)));

		if (mask & (1 << type))
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("found duplicate resource group resource type: %s",
							defel->defname)));
		else
			mask |= 1 << type;

		if (type == RESGROUP_LIMIT_TYPE_CPUSET) 
		{
			const char *cpuset = defGetString(defel);
			checkCpusetSyntax(cpuset);
			StrNCpy(caps->cpuset, cpuset, sizeof(caps->cpuset));
			caps->cpuRateLimit = CPU_RATE_LIMIT_DISABLED;
		}
		else 
		{
			value = getResgroupOptionValue(defel, type);
			checkResgroupCapLimit(type, value);

			switch (type)
			{
				case RESGROUP_LIMIT_TYPE_CONCURRENCY:
					caps->concurrency = value;
					break;
				case RESGROUP_LIMIT_TYPE_CPU:
					caps->cpuRateLimit = value;
					SetCpusetEmpty(caps->cpuset, sizeof(caps->cpuset));
					break;
				case RESGROUP_LIMIT_TYPE_MEMORY:
					caps->memLimit = value;
					break;
				case RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA:
					caps->memSharedQuota = value;
					break;
				case RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO:
					caps->memSpillRatio = value;
					break;
				case RESGROUP_LIMIT_TYPE_MEMORY_AUDITOR:
					caps->memAuditor = value;
					break;
				default:
					break;
			}
		}
	}

	if ((mask & (1 << RESGROUP_LIMIT_TYPE_CPUSET)))
		EnsureCpusetIsAvailable(ERROR);

	if ((mask & (1 << RESGROUP_LIMIT_TYPE_CPU)) &&
		(mask & (1 << RESGROUP_LIMIT_TYPE_CPUSET)))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("can't specify both cpu_rate_limit and cpuset")));

	if (!(mask & (1 << RESGROUP_LIMIT_TYPE_CPU)) &&
		!(mask & (1 << RESGROUP_LIMIT_TYPE_CPUSET)))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("must specify cpu_rate_limit or cpuset")));

	if (!(mask & (1 << RESGROUP_LIMIT_TYPE_CONCURRENCY)))
		caps->concurrency = RESGROUP_DEFAULT_CONCURRENCY;

	if (!(mask & (1 << RESGROUP_LIMIT_TYPE_MEMORY)))
		caps->memLimit = RESGROUP_DEFAULT_MEMORY_LIMIT;

	if (!(mask & (1 << RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA)))
		caps->memSharedQuota = RESGROUP_DEFAULT_MEM_SHARED_QUOTA;

	if (!(mask & (1 << RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO)))
		caps->memSpillRatio = RESGROUP_DEFAULT_MEM_SPILL_RATIO;

	if (!(mask & (1 << RESGROUP_LIMIT_TYPE_MEMORY_AUDITOR)))
		caps->memAuditor = RESGROUP_DEFAULT_MEM_AUDITOR;

	checkResgroupCapConflicts(caps);
}

/*
 * Resource group call back function
 *
 * Remove resource group entry in shared memory when abort transaction which
 * creates resource groups
 */
static void
createResgroupCallback(XactEvent event, void *arg)
{
	ResourceGroupCallbackContext *callbackCtx = arg;

	if (event != XACT_EVENT_COMMIT)
	{
		ResGroupCreateOnAbort(callbackCtx);
	}
	pfree(callbackCtx);
}

/*
 * Resource group call back function
 *
 * When DROP RESOURCE GROUP transaction ends, need wake up
 * the queued transactions and cleanup shared menory entry.
 */
static void
dropResgroupCallback(XactEvent event, void *arg)
{
	ResourceGroupCallbackContext *callbackCtx = arg;

	ResGroupDropFinish(callbackCtx, event == XACT_EVENT_COMMIT);
	pfree(callbackCtx);
}

/*
 * Resource group call back function
 *
 * When ALTER RESOURCE GROUP SET CONCURRENCY commits, some queuing
 * transaction of this resource group may need to be woke up.
 */
static void
alterResgroupCallback(XactEvent event, void *arg)
{
	ResourceGroupCallbackContext *callbackCtx = arg;

	if (event == XACT_EVENT_COMMIT)
		ResGroupAlterOnCommit(callbackCtx);

	pfree(callbackCtx);
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
 * @param caps     the capabilities
 */
static void
insertResgroupCapabilities(Relation rel, Oid groupId, ResGroupCaps *caps)
{
	char value[64];

	snprintf(value, sizeof(value), "%d", caps->concurrency);
	insertResgroupCapabilityEntry(rel, groupId,
								  RESGROUP_LIMIT_TYPE_CONCURRENCY, value);

	snprintf(value, sizeof(value), "%d", caps->cpuRateLimit);
	insertResgroupCapabilityEntry(rel, groupId,
								  RESGROUP_LIMIT_TYPE_CPU, value);

	snprintf(value, sizeof(value), "%d", caps->memLimit);
	insertResgroupCapabilityEntry(rel, groupId,
								  RESGROUP_LIMIT_TYPE_MEMORY, value);

	snprintf(value, sizeof(value), "%d", caps->memSharedQuota);
	insertResgroupCapabilityEntry(rel, groupId,
								  RESGROUP_LIMIT_TYPE_MEMORY_SHARED_QUOTA, value);

	snprintf(value, sizeof(value), "%d", caps->memSpillRatio);
	insertResgroupCapabilityEntry(rel, groupId,
								  RESGROUP_LIMIT_TYPE_MEMORY_SPILL_RATIO, value);

	snprintf(value, sizeof(value), "%d", caps->memAuditor);
	insertResgroupCapabilityEntry(rel, groupId,
								  RESGROUP_LIMIT_TYPE_MEMORY_AUDITOR, value);

	insertResgroupCapabilityEntry(rel, groupId,
								  RESGROUP_LIMIT_TYPE_CPUSET, caps->cpuset);
}

/*
 * Update all the capabilities of one resgroup in pg_resgroupcapability
 *
 * groupId and limitType are the scan keys.
 */
static void
updateResgroupCapabilityEntry(Relation rel,
							  Oid groupId,
							  ResGroupLimitType limitType,
							  ResGroupCap value,
							  const char *strValue)
{
	HeapTuple	oldTuple;
	HeapTuple	newTuple;
	SysScanDesc	sscan;
	ScanKeyData	scankey[2];
	Datum		values[Natts_pg_resgroupcapability];
	bool		isnull[Natts_pg_resgroupcapability];
	bool		repl[Natts_pg_resgroupcapability];
	char		stringBuffer[MaxCpuSetLength];

	ScanKeyInit(&scankey[0],
				Anum_pg_resgroupcapability_resgroupid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(groupId));
	ScanKeyInit(&scankey[1],
				Anum_pg_resgroupcapability_reslimittype,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum(limitType));

	sscan = systable_beginscan(rel,
							   ResGroupCapabilityResgroupidResLimittypeIndexId, true,
							   NULL, 2, scankey);

	MemSet(values, 0, sizeof(values));
	MemSet(isnull, 0, sizeof(isnull));
	MemSet(repl, 0, sizeof(repl));

	oldTuple = systable_getnext(sscan);
	if (!HeapTupleIsValid(oldTuple))
	{
		/*
		 * It's possible for a cap to be missing, e.g. a resgroup is created
		 * with v5.0 which does not support cap=7 (cpuset), then we binary
		 * switch to v5.10 and alter it, then we'll find cap=7 missing here.
		 * Instead of raising an error we should fallback to insert a new cap.
		 */

		systable_endscan(sscan);

		insertResgroupCapabilityEntry(rel, groupId, limitType, (char *) strValue);

		return;
	}

	if (limitType == RESGROUP_LIMIT_TYPE_CPUSET)
	{
		StrNCpy(stringBuffer, strValue, sizeof(stringBuffer));
	}
	else
	{
		snprintf(stringBuffer, sizeof(stringBuffer), "%d", value);
	}

	values[Anum_pg_resgroupcapability_value - 1] = CStringGetTextDatum(stringBuffer);
	isnull[Anum_pg_resgroupcapability_value - 1] = false;
	repl[Anum_pg_resgroupcapability_value - 1]  = true;

	newTuple = heap_modify_tuple(oldTuple, RelationGetDescr(rel),
								 values, isnull, repl);

	CatalogTupleUpdate(rel, &oldTuple->t_self, newTuple);

	systable_endscan(sscan);
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
 * @param caps     the capabilities for the resource group
 */
static void
validateCapabilities(Relation rel,
					 Oid groupid,
					 ResGroupCaps *caps,
					 bool newGroup)
{
	HeapTuple tuple;
	SysScanDesc sscan;
	int totalCpu = caps->cpuRateLimit;
	int totalMem = caps->memLimit;
	char cpusetAll[MaxCpuSetLength] = {0};
	char cpusetMissing[MaxCpuSetLength] = {0};
	Bitmapset *bmsCurrent = NULL;
	Bitmapset *bmsCommon = NULL;

	if (!CpusetIsEmpty(caps->cpuset))
	{
		EnsureCpusetIsAvailable(ERROR);
	}

	/*
	 * initialize the variables only when resource group is activated
	 */
	if (IsResGroupActivated() &&
		gp_resource_group_enable_cgroup_cpuset)
	{
		Bitmapset *bmsAll = NULL;

		/* Get all available cores */
		ResGroupOps_GetCpuSet(RESGROUP_ROOT_ID,
							  cpusetAll,
							  MaxCpuSetLength);
		bmsAll = CpusetToBitset(cpusetAll, MaxCpuSetLength);
		/* Check whether the cores in this group are available */
		if (!CpusetIsEmpty(caps->cpuset))
		{
			Bitmapset *bmsMissing = NULL;

			bmsCurrent = CpusetToBitset(caps->cpuset, MaxCpuSetLength);
			bmsCommon = bms_intersect(bmsCurrent, bmsAll);
			bmsMissing = bms_difference(bmsCurrent, bmsCommon);

			if (!bms_is_empty(bmsMissing))
			{
				BitsetToCpuset(bmsMissing, cpusetMissing, MaxCpuSetLength);

				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("cpu cores %s are unavailable on the system",
								cpusetMissing)));
			}
		}
	}

	sscan = systable_beginscan(rel, ResGroupCapabilityResgroupidIndexId,
							   true, NULL, 0, NULL);

	while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
	{
		Datum				groupIdDatum;
		Datum				typeDatum;
		Datum				valueDatum;
		ResGroupLimitType	reslimittype;
		Oid					resgroupid;
		char				*valueStr;
		int					value;
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
					 errmsg("found duplicate resource group id: %d",
							groupid)));
		}

		typeDatum = heap_getattr(tuple, Anum_pg_resgroupcapability_reslimittype,
								 rel->rd_att, &isNull);
		reslimittype = (ResGroupLimitType) DatumGetInt16(typeDatum);

		valueDatum = heap_getattr(tuple, Anum_pg_resgroupcapability_value,
									 rel->rd_att, &isNull);

		if (reslimittype == RESGROUP_LIMIT_TYPE_CPU)
		{
			valueStr = TextDatumGetCString(valueDatum);
			value = str2Int(valueStr, getResgroupOptionName(reslimittype));
			if (value != CPU_RATE_LIMIT_DISABLED)
			{
				totalCpu += value;
				if (totalCpu > RESGROUP_MAX_CPU_RATE_LIMIT)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							errmsg("total cpu_rate_limit exceeded the limit of %d",
								   RESGROUP_MAX_CPU_RATE_LIMIT)));
			}
		}
		else if (reslimittype == RESGROUP_LIMIT_TYPE_MEMORY)
		{
			valueStr = TextDatumGetCString(valueDatum);
			value = str2Int(valueStr, getResgroupOptionName(reslimittype));
			totalMem += value;
			if (totalMem > RESGROUP_MAX_MEMORY_LIMIT)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						errmsg("total memory_limit exceeded the limit of %d",
							   RESGROUP_MAX_MEMORY_LIMIT)));
		}
		else if (reslimittype == RESGROUP_LIMIT_TYPE_CPUSET)
		{
			/*
			 * do the check when resource group is activated
			 */
			if (IsResGroupActivated() && !CpusetIsEmpty(caps->cpuset))
			{
				valueStr = TextDatumGetCString(valueDatum);
				if (!CpusetIsEmpty(valueStr))
				{
					Bitmapset *bmsOther = NULL;

					EnsureCpusetIsAvailable(ERROR);

					Assert(!bms_is_empty(bmsCurrent));

					bmsOther = CpusetToBitset(valueStr, MaxCpuSetLength);
					bmsCommon = bms_intersect(bmsCurrent, bmsOther);

					if (!bms_is_empty(bmsCommon))
					{
						BitsetToCpuset(bmsCommon, cpusetMissing, MaxCpuSetLength);

						ereport(ERROR,
								(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
								 errmsg("cpu cores %s are used by resource group %s",
										cpusetMissing,
										GetResGroupNameForId(resgroupid))));
					}
				}
			}
		}
	}

	systable_endscan(sscan);
}

/*
 * Insert one capability to the capability table.
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
							 const char *value)
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

	tuple = heap_form_tuple(tupleDesc, new_record, new_record_nulls);
	CatalogTupleInsert(rel, tuple);
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
							   true, NULL, 1, &scankey);

	while ((tuple = systable_getnext(sscan)) != NULL)
		simple_heap_delete(resgroup_capability_rel, &tuple->t_self);

	systable_endscan(sscan);

	heap_close(resgroup_capability_rel, NoLock);
}

/*
 * Check to see if any roles are in this resource group.
 */
static void
checkAuthIdForDrop(Oid groupId)
{
	Relation	 authIdRel;
	ScanKeyData	 authidScankey;
	SysScanDesc	 authidScan;

	authIdRel = heap_open(AuthIdRelationId, RowExclusiveLock);
	ScanKeyInit(&authidScankey,
				Anum_pg_authid_rolresgroup,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(groupId));

	authidScan = systable_beginscan(authIdRel, AuthIdRolResGroupIndexId, true,
									NULL, 1, &authidScankey);

	if (HeapTupleIsValid(systable_getnext(authidScan)))
		ereport(ERROR,
				(errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
				 errmsg("resource group is used by at least one role")));

	systable_endscan(authidScan);
	heap_close(authIdRel, RowExclusiveLock);
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

/*
 * Get memory auditor from auditor name.
 */
static int
getResGroupMemAuditor(char *name)
{
	int index;

	for (index = 0; index < RESGROUP_MEMORY_AUDITOR_COUNT; index ++)
	{
		if (strcmp(ResGroupMemAuditorName[index], name) == 0)
			return index;
	}

	return RESGROUP_INVALID_MEM_AUDITOR;
}

/*
 * check whether the cpuset value is syntactically right
 */
static bool
checkCpusetSyntax(const char *cpuset)
{
	if (strlen(cpuset) >= MaxCpuSetLength)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("the length of cpuset reached the upper limit %d",
						MaxCpuSetLength)));
		return false;
	}
	if (!CpusetToBitset(cpuset,
						 strlen(cpuset)))
	{
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("cpuset invalid")));
		return false;
	}
	return true;
}

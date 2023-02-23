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
#include "utils/cgroup.h"
#include "utils/resource_manager.h"
#include "utils/resowner.h"
#include "utils/syscache.h"
#include "utils/faultinjector.h"

#define RESGROUP_DEFAULT_CONCURRENCY (20)
#define RESGROUP_DEFAULT_CPU_SOFT_PRIORITY (100)

#define RESGROUP_MIN_CONCURRENCY	(0)
#define RESGROUP_MAX_CONCURRENCY	(MaxConnections)

#define RESGROUP_MAX_CPU_HARD_QUOTA_LIMIT	(100)
#define RESGROUP_MIN_CPU_HARD_QUOTA_LIMIT	(1)

#define RESGROUP_MIN_CPU_SOFT_PRIORITY	(1)
#define RESGROUP_MAX_CPU_SOFT_PRIORITY	(500)

static int str2Int(const char *str, const char *prop);
static ResGroupLimitType getResgroupOptionType(const char* defname);
static ResGroupCap getResgroupOptionValue(DefElem *defel);
static const char *getResgroupOptionName(ResGroupLimitType type);
static void checkResgroupCapLimit(ResGroupLimitType type, ResGroupCap value);
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
static void checkCpusetSyntax(const char *cpuset);


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
	 * Check for an illegal name ('none' is used to signify no group in ALTER ROLE).
	 */
	if (strcmp(stmt->name, "none") == 0)
		ereport(ERROR,
				(errcode(ERRCODE_RESERVED_NAME),
				 errmsg("resource group name \"none\" is reserved")));

	ClearResGroupCaps(&caps);
	parseStmtOptions(stmt, &caps);

	/*
	 * Both CREATE and ALTER resource group need check the intersection of cpuset,
	 * to make it simple, acquire ExclusiveLock lock on pg_resgroupcapability at
	 * the beginning of CREATE and ALTER.
	 */
	pg_resgroupcapability_rel = table_open(ResGroupCapabilityRelationId, ExclusiveLock);
	pg_resgroup_rel = table_open(ResGroupRelationId, RowExclusiveLock);

	sscan = systable_beginscan(pg_resgroup_rel, ResGroupRsgnameIndexId, false,
							   NULL, 0, NULL);
	nResGroups = 0;
	while (systable_getnext(sscan) != NULL)
		nResGroups++;
	systable_endscan(sscan);

	/* Check if MaxResourceGroups limit is reached */
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
		cgroupOpsRoutine->createcgroup(groupid);

		if (CpusetIsEmpty(caps.cpuset))
		{
			cgroupOpsRoutine->setcpulimit(groupid, caps.cpuHardQuotaLimit);
			cgroupOpsRoutine->setcpupriority(groupid, caps.cpuSoftPriority);
		}
		else
		{
			EnsureCpusetIsAvailable(ERROR);

			char *cpuset = getCpuSetByRole(caps.cpuset);
			cgroupOpsRoutine->setcpuset(groupid, cpuset);
			/* reset default group, subtract new group cpu cores */
			char defaultGroupCpuset[MaxCpuSetLength];
			cgroupOpsRoutine->getcpuset(DEFAULT_CPUSET_GROUP_ID,
								  defaultGroupCpuset,
								  MaxCpuSetLength);
			CpusetDifference(defaultGroupCpuset, cpuset, MaxCpuSetLength);
			cgroupOpsRoutine->setcpuset(DEFAULT_CPUSET_GROUP_ID, defaultGroupCpuset);
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
	if (groupid == DEFAULTRESGROUP_OID
		|| groupid == ADMINRESGROUP_OID
		|| groupid == SYSTEMRESGROUP_OID)
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
		checkCpuSetByRole(cpuset);
	}
	else
	{
		value = getResgroupOptionValue(defel);
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
	 * We use ExclusiveLock here to prevent concurrent increase on different
	 * resource group.
	 *
	 * We can't use AccessExclusiveLock here, the reason is that, if there is
	 * a database recovery happened when run "alter resource group" and acquire
	 * this kind of lock, the initialization of resource group in function
	 * InitResGroups will be pending during database startup, since this function
	 * will open this table with AccessShareLock, AccessExclusiveLock is not
	 * compatible with any other lock. ExclusiveLock and AccessShareLock are
	 * compatible.
	 */
	pg_resgroupcapability_rel = heap_open(ResGroupCapabilityRelationId,
										  ExclusiveLock);

	/* Load current resource group capabilities */
	GetResGroupCapabilities(pg_resgroupcapability_rel, groupid, &oldCaps);
	caps = oldCaps;

	switch (limitType)
	{
		case RESGROUP_LIMIT_TYPE_CPU:
			caps.cpuHardQuotaLimit = value;
			SetCpusetEmpty(caps.cpuset, sizeof(caps.cpuset));
			break;
		case RESGROUP_LIMIT_TYPE_CPU_SHARES:
			caps.cpuSoftPriority = value;
			break;
		case RESGROUP_LIMIT_TYPE_CONCURRENCY:
			caps.concurrency = value;
			break;
		case RESGROUP_LIMIT_TYPE_CPUSET:
			StrNCpy(caps.cpuset, cpuset, sizeof(caps.cpuset));
			caps.cpuHardQuotaLimit = CPU_HARD_QUOTA_LIMIT_DISABLED;
			caps.cpuSoftPriority = RESGROUP_DEFAULT_CPU_SOFT_PRIORITY;
			break;
		case RESGROUP_LIMIT_TYPE_MEMORY_LIMIT:
			caps.memory_limit = value;
			break;
		default:
			break;
	}

	validateCapabilities(pg_resgroupcapability_rel, groupid, &caps, false);

	/* cpuset & cpu_hard_quota_limit can not coexist.
	 * if cpuset is active, then cpu_hard_quota_limit must set to CPU_RATE_LIMIT_DISABLED,
	 * if cpu_hard_quota_limit is active, then cpuset must set to "" */
	if (limitType == RESGROUP_LIMIT_TYPE_CPUSET)
	{
		updateResgroupCapabilityEntry(pg_resgroupcapability_rel,
									  groupid, RESGROUP_LIMIT_TYPE_CPU,
									  CPU_HARD_QUOTA_LIMIT_DISABLED, "");
		updateResgroupCapabilityEntry(pg_resgroupcapability_rel,
									  groupid, RESGROUP_LIMIT_TYPE_CPU_SHARES,
									  RESGROUP_DEFAULT_CPU_SOFT_PRIORITY, "");

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
				resgroupCaps->cpuHardQuotaLimit = str2Int(value,
														  getResgroupOptionName(type));
				break;
			case RESGROUP_LIMIT_TYPE_CPU_SHARES:
				resgroupCaps->cpuSoftPriority = str2Int(value,
														getResgroupOptionName(type));
				break;
			case RESGROUP_LIMIT_TYPE_CPUSET:
				StrNCpy(resgroupCaps->cpuset, value, sizeof(resgroupCaps->cpuset));
				break;
			case RESGROUP_LIMIT_TYPE_MEMORY_LIMIT:
				resgroupCaps->memory_limit = str2Int(value,
													getResgroupOptionName(type));
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
	if (strcmp(defname, "cpu_hard_quota_limit") == 0)
		return RESGROUP_LIMIT_TYPE_CPU;
	else if (strcmp(defname, "concurrency") == 0)
		return RESGROUP_LIMIT_TYPE_CONCURRENCY;
	else if (strcmp(defname, "cpuset") == 0)
		return RESGROUP_LIMIT_TYPE_CPUSET;
	else if (strcmp(defname, "cpu_soft_priority") == 0)
		return RESGROUP_LIMIT_TYPE_CPU_SHARES;
	else if (strcmp(defname, "memory_limit") == 0)
		return RESGROUP_LIMIT_TYPE_MEMORY_LIMIT;
	else
		return RESGROUP_LIMIT_TYPE_UNKNOWN;
}

/*
 * Get capability value from DefElem, convert from int64 to int
 */
static ResGroupCap
getResgroupOptionValue(DefElem *defel)
{
	int64 value;

	value = defGetInt64(defel);

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
			return "cpu_hard_quota_limit";
		case RESGROUP_LIMIT_TYPE_CPUSET:
			return "cpuset";
		case RESGROUP_LIMIT_TYPE_CPU_SHARES:
			return "cpu_soft_priority";
		case RESGROUP_LIMIT_TYPE_MEMORY_LIMIT:
			return "memory_limit";
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
				if (value > RESGROUP_MAX_CPU_HARD_QUOTA_LIMIT ||
					(value < RESGROUP_MIN_CPU_HARD_QUOTA_LIMIT && value != CPU_HARD_QUOTA_LIMIT_DISABLED))
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							errmsg("cpu_hard_quota_limit range is [%d, %d] or equals to %d",
								   RESGROUP_MIN_CPU_HARD_QUOTA_LIMIT, RESGROUP_MAX_CPU_HARD_QUOTA_LIMIT,
								   CPU_HARD_QUOTA_LIMIT_DISABLED)));
				break;

			case RESGROUP_LIMIT_TYPE_CPU_SHARES:
				if (value < RESGROUP_MIN_CPU_SOFT_PRIORITY ||
					value > RESGROUP_MAX_CPU_SOFT_PRIORITY)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
									errmsg("cpu_soft_priority range is [%d, %d]",
										   RESGROUP_MIN_CPU_SOFT_PRIORITY, RESGROUP_MAX_CPU_SOFT_PRIORITY)));
				break;

			case RESGROUP_LIMIT_TYPE_MEMORY_LIMIT:
				break;

			default:
				Assert(!"unexpected options");
				break;
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
			StrNCpy(caps->cpuset, cpuset, sizeof(caps->cpuset));
      checkCpuSetByRole(cpuset);
			caps->cpuHardQuotaLimit = CPU_HARD_QUOTA_LIMIT_DISABLED;
			caps->cpuSoftPriority = RESGROUP_DEFAULT_CPU_SOFT_PRIORITY;
		}
		else 
		{
			value = getResgroupOptionValue(defel);
			checkResgroupCapLimit(type, value);

			switch (type)
			{
				case RESGROUP_LIMIT_TYPE_CONCURRENCY:
					caps->concurrency = value;
					break;
				case RESGROUP_LIMIT_TYPE_CPU:
					caps->cpuHardQuotaLimit = value;
					SetCpusetEmpty(caps->cpuset, sizeof(caps->cpuset));
					break;
				case RESGROUP_LIMIT_TYPE_CPU_SHARES:
					caps->cpuSoftPriority = value;
					break;
				case RESGROUP_LIMIT_TYPE_MEMORY_LIMIT:
					caps->memory_limit = value;
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
				errmsg("can't specify both cpu_hard_quota_limit and cpuset")));

	if (!(mask & (1 << RESGROUP_LIMIT_TYPE_CPU)) &&
		!(mask & (1 << RESGROUP_LIMIT_TYPE_CPUSET)))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("must specify cpu_hard_quota_limit or cpuset")));

	if (!(mask & (1 << RESGROUP_LIMIT_TYPE_CONCURRENCY)))
		caps->concurrency = RESGROUP_DEFAULT_CONCURRENCY;

	if (!(mask & (1 << RESGROUP_LIMIT_TYPE_MEMORY_LIMIT)))
		caps->memory_limit = -1;

	if ((mask & (1 << RESGROUP_LIMIT_TYPE_CPU)) &&
		!(mask & (1 << RESGROUP_LIMIT_TYPE_CPU_SHARES)))
		caps->cpuSoftPriority = RESGROUP_DEFAULT_CPU_SOFT_PRIORITY;
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

	snprintf(value, sizeof(value), "%d", caps->cpuHardQuotaLimit);
	insertResgroupCapabilityEntry(rel, groupId,
								  RESGROUP_LIMIT_TYPE_CPU, value);

	snprintf(value, sizeof(value), "%d", caps->cpuSoftPriority);
	insertResgroupCapabilityEntry(rel, groupId,
								  RESGROUP_LIMIT_TYPE_CPU_SHARES, value);

	insertResgroupCapabilityEntry(rel, groupId,
								  RESGROUP_LIMIT_TYPE_CPUSET, caps->cpuset);

	snprintf(value, sizeof(value), "%d", caps->memory_limit);
	insertResgroupCapabilityEntry(rel, groupId,
								  RESGROUP_LIMIT_TYPE_MEMORY_LIMIT, value);
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
		Bitmapset *bmsMissing = NULL;

		/* Get all available cores */
		cgroupOpsRoutine->getcpuset(CGROUP_ROOT_ID,
							  cpusetAll,
							  MaxCpuSetLength);
		bmsAll = CpusetToBitset(cpusetAll, MaxCpuSetLength);

		/* Check whether the cores in this group are available */
		if (!CpusetIsEmpty(caps->cpuset))
		{
			char *cpuset = getCpuSetByRole(caps->cpuset);
			bmsCurrent = CpusetToBitset(cpuset, MaxCpuSetLength);

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

		/* we need to check the configuration of cpuset for intersection. */
		if (reslimittype == RESGROUP_LIMIT_TYPE_CPUSET)
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

					char *cpuset = getCpuSetByRole(valueStr);
					bmsOther = CpusetToBitset(cpuset, MaxCpuSetLength);
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
 * check whether the cpuset value is syntactically right
 */
static void
checkCpusetSyntax(const char *cpuset)
{
	if (cpuset == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("cpuset invalid")));
	}

	if (strlen(cpuset) >= MaxCpuSetLength)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("the length of cpuset reached the upper limit %d",
						MaxCpuSetLength)));
	}
	if (!CpusetToBitset(cpuset,
						 strlen(cpuset)))
	{
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("cpuset invalid")));
	}

	return;
}

/*
 * Check Cpuset by coordinator and segment
 */
extern void
checkCpuSetByRole(const char *cpuset)
{
	char *first = NULL;
	char *last = NULL;

	if (cpuset == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				errmsg("cpuset invalid")));
	}

	first = strchr(cpuset, ';');
	last = strrchr(cpuset, ';');
	/*
	 * If point first not equal last, that means
	 * ';' character exceed limit numbers.
	 */
	if (last != first)
	{
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				errmsg("cpuset invalid")));
	}

	if (first == NULL)
		checkCpusetSyntax(cpuset);
	else
	{
		char mcpu[MaxCpuSetLength] = {0};
		strncpy(mcpu, cpuset, first - cpuset);

		checkCpusetSyntax(mcpu);
		checkCpusetSyntax(first + 1);
	}

	return;
}

/*
 * Seperate cpuset by coordinator and segment
 * Return as splitcpuset
 *
 * ex:
 * cpuset = "1;4"
 * then we should assign '1' to corrdinator and '4' to segment
 * 
 * cpuset = "1"
 * assign '1' to both coordinator and segment
 */
extern char *
getCpuSetByRole(const char *cpuset)
{
	char *splitcpuset = NULL;

	if (cpuset == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				errmsg("Unexpected cpuset invalid in getCpuSetByRole")));
	}

	char *first = strchr(cpuset, ';');
	if (first == NULL)
		splitcpuset = (char *)cpuset;
	else
	{
		char *scpu = first + 1;

		/* Get result cpuset by IS_QUERY_DISPATCHER(), on master or segment */
		if (IS_QUERY_DISPATCHER())
			splitcpuset = scpu;
		else
		{
			char *mcpu = (char *)palloc0(sizeof(char) * MaxCpuSetLength);
			strncpy(mcpu, cpuset, first - cpuset);
			splitcpuset = mcpu;
		}
	}

	return splitcpuset;
}

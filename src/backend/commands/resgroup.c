/*-------------------------------------------------------------------------
 *
 * resgroup.c
 *	  Commands for manipulating resource group.
 *
 * Copyright (c) 2006-2017, Greenplum inc.
 *
 * IDENTIFICATION
 *    src/backend/commands/resgroup.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "catalog/heap.h"
#include "catalog/oid_dispatch.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_resgroup.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbvars.h"
#include "commands/comment.h"
#include "commands/defrem.h"
#include "commands/resgroup.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"

#define RESGROUP_DEFAULT_CONCURRENCY (20)
#define RESGROUP_DEFAULT_REDZONE_LIMIT (0.8)


/**
 * Internal struct to store the group settings.
 */
typedef struct ResourceGroupOptions
{
	int concurrency;
	float cpuRateLimit;
	float memoryLimit;
	float redzoneLimit;
} ResourceGroupOptions;

static float floatFromText(const text* text, const char * prop);
static void updateResgroupCapability(Oid groupid, ResourceGroupOptions *options);
static void deleteResgroupCapability(Oid groupid);
static int getResgroupOptionType(const char* defname);
static void parseStmtOptions(CreateResourceGroupStmt *stmt, ResourceGroupOptions *options);
static void validateCapabilities(Relation rel, Oid groupid, ResourceGroupOptions *options);

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
	 * Check the pg_resgroup relation to be certain the group doesn't already
	 * exist.
	 */
	pg_resgroup_rel = heap_open(ResGroupRelationId, RowExclusiveLock);
	pg_resgroup_dsc = RelationGetDescr(pg_resgroup_rel);

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

	tuple = heap_form_tuple(pg_resgroup_dsc, new_record, new_record_nulls);

	/*
	 * Insert new record in the pg_resgroup table
	 */
	groupid = simple_heap_insert(pg_resgroup_rel, tuple);
	CatalogUpdateIndexes(pg_resgroup_rel, tuple);

	/* process the WITH (...) list items */
	updateResgroupCapability(groupid, &options);

	/* TODO: create the actual objects in shared memory */

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

	/* TODO: delete the actual objects in shared memory */

	/*
	 * Remove any comments on this resource group
	 */
	DeleteSharedComments(groupid, ResGroupRelationId);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR|
									DF_WITH_SNAPSHOT|
									DF_NEED_TWO_PHASE,
									NIL, /* FIXME */
									NULL);
	}

	/* metadata tracking */
	MetaTrackDropObject(ResGroupRelationId, groupid);

	/* drop the extended attributes for this resource group */
	deleteResgroupCapability(groupid);

	heap_close(pg_resgroup_rel, NoLock);
}

/**
 * Convert a text to a float value.
 *
 * @param text  the text
 * @param prop  the property name
 */
static float
floatFromText(const text* text, const char* prop)
{
	char * str = DatumGetCString(DirectFunctionCall1(textout,
								 PointerGetDatum(text)));
	char * end = NULL;
	double val = strtod(str, &end);

	/* both the property name and value are already checked
	 * by the syntax parser, but we'll check it again anyway for safe. */
	if (end == NULL || end == str || *end != 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("%s requires a numeric value", prop)));

	return (float) val;
}

/**
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
insertTupleIntoResCapability(Relation rel,
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

/**
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
updateResgroupCapability(Oid groupid,
						 ResourceGroupOptions *options)
{
	char value[64];
	Relation resgroup_capability_rel = heap_open(ResGroupCapabilityRelationId, RowExclusiveLock);

	validateCapabilities(resgroup_capability_rel, groupid, options);

	sprintf(value, "%d", options->concurrency);
	insertTupleIntoResCapability(resgroup_capability_rel, groupid, RESGROUP_LIMIT_TYPE_CONCURRENCY, value);

	sprintf(value, "%.2f", options->cpuRateLimit);
	insertTupleIntoResCapability(resgroup_capability_rel, groupid, RESGROUP_LIMIT_TYPE_CPU, value);

	sprintf(value, "%.2f", options->memoryLimit);
	insertTupleIntoResCapability(resgroup_capability_rel, groupid, RESGROUP_LIMIT_TYPE_MEMORY, value);

	sprintf(value, "%.2f", options->redzoneLimit);
	insertTupleIntoResCapability(resgroup_capability_rel, groupid, RESGROUP_LIMIT_TYPE_MEMORY_REDZONE, value);

	heap_close(resgroup_capability_rel, NoLock);
}

static void
deleteResgroupCapability(Oid groupid)
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

/**
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

/**
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

/**
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
			totalCpu += floatFromText(&resgCapability->value, "cpu_rate_limit");
			if (totalCpu > 1.0)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						errmsg("total cpu_rate_limit exceeded the limit of 1.0")));
		}
		else if (resgCapability->reslimittype == RESGROUP_LIMIT_TYPE_MEMORY)
		{
			totalMem += floatFromText(&resgCapability->value, "memory_limit");
			if (totalMem > 1.0)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						errmsg("total memory_limit exceeded the limit of 1.0")));
		}
	}

	systable_endscan(sscan);
}

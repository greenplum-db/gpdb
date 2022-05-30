/*
 * ---------------------------------------------------------------------------
 *
 *     Greenplum Database
 *     Copyright (C) 2020 VMware, Inc.
 *
 *     @filename:
 *             gp_resource_group.c
 *
 *     @doc:
 *             Implementation of pg_resgroup_move_query
 *
 * ---------------------------------------------------------------------------
 */
#include "postgres.h"
#include "access/tupdesc.h"
#include "access/htup.h"
#include "cdb/cdbvars.h"
#include "commands/resgroupcmds.h"
#include "fmgr.h"
#include "funcapi.h"
#include "storage/procarray.h"
#include "utils/builtins.h"
#include "utils/resgroup.h"
#include "utils/resource_manager.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

extern Datum pg_resgroup_check_move_query(PG_FUNCTION_ARGS);
extern Datum pg_resgroup_move_query(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(pg_resgroup_check_move_query);
PG_FUNCTION_INFO_V1(pg_resgroup_move_query);

Datum
pg_resgroup_check_move_query(PG_FUNCTION_ARGS)
{
	TupleDesc	tupdesc;
	Datum		values[2];
	bool		nulls[2];
	HeapTuple	htup;
	int sessionId = PG_GETARG_INT32(0);
	Oid groupId = PG_GETARG_OID(1);
	int32 sessionMem = ResGroupGetSessionMemUsage(sessionId);
	int32 availMem = ResGroupGetGroupAvailableMem(groupId);

	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");
	tupdesc = BlessTupleDesc(tupdesc);

	MemSet(nulls, 0, sizeof(nulls));
	values[0] = Int32GetDatum(sessionMem);
	values[1] = Int32GetDatum(availMem);
	htup = heap_form_tuple(tupdesc, values, nulls);

	PG_RETURN_DATUM(HeapTupleGetDatum(htup));
}

/*
 * move a query to a resource group
 */
Datum
pg_resgroup_move_query(PG_FUNCTION_ARGS)
{
	int sessionId;
	Oid groupId;
	const char *groupName;

	if (!IsResGroupEnabled())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("resource group is not enabled"))));

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("must be superuser to move query"))));

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		Oid currentGroupId;
		pid_t pid = PG_GETARG_INT32(0);
		groupName = text_to_cstring(PG_GETARG_TEXT_PP(1));

		if (pid == MyProcPid)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 (errmsg("cannot move myself"))));

		groupId = GetResGroupIdForName(groupName);
		if (groupId == InvalidOid)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 (errmsg("cannot find resource group: %s", groupName))));

		sessionId = GetSessionIdByPid(pid);
		if (sessionId == -1)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 (errmsg("cannot find process: %d", pid))));

		currentGroupId = ResGroupGetGroupIdBySessionId(sessionId);
		if (currentGroupId == InvalidOid)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 (errmsg("process %d is in IDLE state", pid))));
		if (currentGroupId == groupId)
			PG_RETURN_BOOL(true);

		ResGroupMoveQuery(sessionId, groupId, groupName);
	}
	else if (Gp_role == GP_ROLE_EXECUTE)
	{
		sessionId = PG_GETARG_INT32(0);
		groupName = text_to_cstring(PG_GETARG_TEXT_PP(1));
		groupId = GetResGroupIdForName(groupName);
		Assert(groupId != InvalidOid);
		if (!ResGroupMoveSignalTarget(sessionId, NULL, groupId, true))
			elog(NOTICE, "cannot send signal to QE; ignoring...");
	}

	PG_RETURN_BOOL(true);
}

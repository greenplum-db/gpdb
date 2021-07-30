/*
 * cdbendpointutils.c
 *
 * Utility functions for endpoints implementation.
 *
 * Copyright (c) 2020-Present VMware, Inc. or its affiliates
 *
 * IDENTIFICATION
 *		src/backend/cdb/cdbendpointutils.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "funcapi.h"
#include "libpq-fe.h"
#include "utils/builtins.h"
#include "utils/portal.h"
#ifdef FAULT_INJECTOR
#include "utils/faultinjector.h"
#endif
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbendpoint.h"
#include "cdbendpoint_private.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"

/* The two struct are used by gp_endpoints(). */

typedef struct
{
	char		name[NAMEDATALEN];
	char		cursorName[NAMEDATALEN];
	int8		token[ENDPOINT_TOKEN_HEX_LEN];
	int			dbid;
	EndpointState state;
	Oid			userId;
	int			sessionId;
}	EndpointInfo;

typedef struct
{
	int			cur_idx;	/* current endpoint info for SRF. */
	EndpointInfo *infos;	/* array of all endpoint info */
	int			total_num;	/* number of endpoints */
}	AllEndpointsInfo;

/* Used in UDFs */
static EndpointState state_string_to_enum(const char *state);
static bool check_parallel_retrieve_cursor(const char *cursorName, bool wait);

/*
 * Convert the string-format token to int (e.g. "123456789" to 0x123456789).
 */
void
endpoint_token_str2hex(int8 *token, const char *tokenStr)
{
	if (strlen(tokenStr) == ENDPOINT_TOKEN_STR_LEN)
		hex_decode(tokenStr, ENDPOINT_TOKEN_STR_LEN, (char *) token);
	else
		ereport(FATAL,
				(errcode(ERRCODE_INVALID_PASSWORD),
				 errmsg("Retrieve auth token is invalid")));
}

/*
 * Convert the hex-format token to string (e.g. 0x123456789 to "123456789").
 */
void
endpoint_token_hex2str(const int8 *token, char *tokenStr)
{
	hex_encode((const char *) token, ENDPOINT_TOKEN_HEX_LEN, tokenStr);
	tokenStr[ENDPOINT_TOKEN_STR_LEN] = 0;
}

/*
 * Returns true if the two given endpoint tokens are equal.
 */
bool
endpoint_token_hex_equals(const int8 *token1, const int8 *token2)
{
	/*
	 * memcmp should be good enough. Timing attack would not be a concern
	 * here.
	 */
	return memcmp(token1, token2, ENDPOINT_TOKEN_HEX_LEN) == 0;
}

bool
endpoint_name_equals(const char *name1, const char *name2)
{
	return strncmp(name1, name2, NAMEDATALEN) == 0;
}

/*
 * gp_check_parallel_retrieve_cursor
 *
 * Check whether given parallel retrieve cursor is finished immediately.
 *
 * Return true means finished.
 * Error out when the cursor has exception raised.
 */
Datum
gp_check_parallel_retrieve_cursor(PG_FUNCTION_ARGS)
{
	const char *cursorName = NULL;

	cursorName = text_to_cstring(PG_GETARG_TEXT_P(0));

	PG_RETURN_BOOL(check_parallel_retrieve_cursor(cursorName, false));
}

/*
 * gp_wait_parallel_retrieve_cursor
 *
 * Wait until the given parallel retrieve cursor is finished.
 *
 * Return true when the cursor finishes.
 * Error out when the cursor has exception raised.
 */
Datum
gp_wait_parallel_retrieve_cursor(PG_FUNCTION_ARGS)
{
	const char *cursorName = NULL;

	cursorName = text_to_cstring(PG_GETARG_TEXT_P(0));

	PG_RETURN_BOOL(check_parallel_retrieve_cursor(cursorName, true));
}

/*
 * check_parallel_retrieve_cursor
 *
 * Support function for UDFs:
 * gp_check_parallel_retrieve_cursor
 * gp_wait_parallel_retrieve_cursor
 *
 * Check whether given parallel retrieve cursor is finishes.
 * If wait is true, hang until parallel retrieve cursor finished.
 *
 * Return true means finished. Error out when parallel retrieve cursor has
 * exception raised.
 */
static bool
check_parallel_retrieve_cursor(const char *cursorName, bool wait)
{
	bool		retVal = false;
	Portal		portal;
	EState	   *estate = NULL;

	/* get the portal from the portal name */
	portal = GetPortalByName(cursorName);
	if (!PortalIsValid(portal))
	{
		ereport(ERROR, (errcode(ERRCODE_UNDEFINED_CURSOR),
						errmsg("cursor \"%s\" does not exist", cursorName)));
		return false;
	}
	if (!PortalIsParallelRetrieveCursor(portal))
	{
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
			   errmsg("this UDF only works for PARALLEL RETRIEVE CURSOR.")));
		return false;
	}

	estate = portal->queryDesc->estate;
	retVal = cdbdisp_checkDispatchAckMessage(estate->dispatcherState, ENDPOINT_FINISHED_ACK_MSG, wait, DISPATCH_WAIT_ACK_ROOT);
	SIMPLE_FAULT_INJECTOR("check_parallel_retrieve_cursor_after_udf");
	check_parallel_retrieve_cursor_errors(estate);

	return retVal;
}

/*
 * check_parallel_retrieve_cursor_errors - Check the PARALLEL RETRIEVE CURSOR
 * execution status. If get error, then rethrow the error.
 */
void
check_parallel_retrieve_cursor_errors(EState *estate)
{
	CdbDispatcherState *ds;
	ErrorData  *qeError = NULL;

	ds = estate->dispatcherState;

	/* Wait for QEs to finish and check their results. */
	cdbdisp_getDispatchResults(ds, &qeError);

	if (qeError != NULL)
	{
		estate->dispatcherState = NULL;
		cdbdisp_cancelDispatch(ds);
		FlushErrorState();
		ReThrowError(qeError);
	}
}

/*
 * On QD, display all the endpoints information in shared memory.
 *
 * Note:
 * As a superuser, it can list all endpoints info of all users', but for
 * non-superuser, it can only list the current user's endpoints info for
 * security reason.
 */
Datum
gp_endpoints(PG_FUNCTION_ARGS)
{
	if (Gp_role != GP_ROLE_DISPATCH)
		ereport(ERROR, (errcode(ERRCODE_GP_COMMAND_ERROR),
						errmsg("gp_endpoints() can be called on query dispatcher only")));

	FuncCallContext *funcctx;
	AllEndpointsInfo *all_info;
	MemoryContext oldcontext;
	Datum		values[9];
	bool		nulls[9];
	HeapTuple	tuple;
	int			res_number, idx;

	if (SRF_IS_FIRSTCALL())
	{
		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/* switch to memory context appropriate for multiple function calls */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* build tuple descriptor */
		TupleDesc	tupdesc =
		CreateTemplateTupleDesc(9);

		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "dbid", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "auth_token", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "cursorname", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "sessionid", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "hostname", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "port", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "userid", OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 8, "state", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 9, "endpointname", TEXTOID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);
		all_info = (AllEndpointsInfo *) palloc0(sizeof(AllEndpointsInfo));
		funcctx->user_fctx = (void *) all_info;
		all_info->cur_idx = 0;
		all_info->infos = NULL;
		all_info->total_num = 0;

		CdbPgResults cdb_pgresults = {NULL, 0};

		CdbDispatchCommand("SELECT endpointname,cursorname,auth_token,dbid,"
						   "state,userid, sessionid"
						   " FROM pg_catalog.gp_segment_endpoints()",
						   DF_WITH_SNAPSHOT | DF_CANCEL_ON_ERROR, &cdb_pgresults);

		if (cdb_pgresults.numResults == 0)
		{
			ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("gp_segment_endpoints didn't get back any data "
								"from the segDBs")));
		}

		res_number = 0;
		for (int i = 0; i < cdb_pgresults.numResults; i++)
		{
			if (PQresultStatus(cdb_pgresults.pg_results[i]) != PGRES_TUPLES_OK)
			{
				cdbdisp_clearCdbPgResults(&cdb_pgresults);
				ereport(
						ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg(
					 "gp_segment_endpoints(): resultStatus is not tuples_Ok")));
			}
			res_number += PQntuples(cdb_pgresults.pg_results[i]);
		}

		if (res_number > 0)
		{
			all_info->infos =
				(EndpointInfo *) palloc0(sizeof(EndpointInfo) * res_number);
			all_info->total_num = res_number;

			for (int i = 0, idx = 0; i < cdb_pgresults.numResults; i++)
			{
				struct pg_result *result = cdb_pgresults.pg_results[i];

				for (int j = 0; j < PQntuples(result); j++)
				{
					StrNCpy(all_info->infos[idx].name, PQgetvalue(result, j, 0), NAMEDATALEN);
					StrNCpy(all_info->infos[idx].cursorName, PQgetvalue(result, j, 1), NAMEDATALEN);
					endpoint_token_str2hex(all_info->infos[idx].token, PQgetvalue(result, j, 2));
					all_info->infos[idx].dbid = atoi(PQgetvalue(result, j, 3));
					all_info->infos[idx].state = state_string_to_enum(PQgetvalue(result, j, 4));
					all_info->infos[idx].userId = (Oid) strtoul(PQgetvalue(result, j, 5), NULL, 10);
					all_info->infos[idx].sessionId = atoi(PQgetvalue(result, j, 6));
					idx++;
				}
			}
		}

		/* get endpoint info on the coordinator */
		LWLockAcquire(ParallelCursorEndpointLock, LW_SHARED);
		int			cnt = 0;

		for (int i = 0; i < MAX_ENDPOINT_SIZE; i++)
		{
			const Endpoint entry = get_endpointdesc_by_index(i);

			if (!entry->empty && (superuser() || entry->userID == GetUserId()))
				cnt++;
		}
		if (cnt != 0)
		{
			idx = all_info->total_num;

			all_info->total_num += cnt;
			if (all_info->infos)
			{
				all_info->infos =
					(EndpointInfo *) repalloc(all_info->infos,
												sizeof(EndpointInfo) * all_info->total_num);
			}
			else
			{
				all_info->infos =
					(EndpointInfo *) palloc(sizeof(EndpointInfo) * all_info->total_num);
			}

			for (int i = 0; i < MAX_ENDPOINT_SIZE; i++)
			{
				const Endpoint entry = get_endpointdesc_by_index(i);

				/*
				 * Only allow current user to get own endpoints. Or let
				 * superuser get all endpoints.
				 */
				if (!entry->empty && (superuser() || entry->userID == GetUserId()))
				{
					EndpointInfo *info = &all_info->infos[idx];

					info->dbid =
						contentid_get_dbid(MASTER_CONTENT_ID,
										   GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY,
										   false);
					get_token_from_session_hashtable(entry->sessionID, entry->userID,
											info->token);
					StrNCpy(info->name, entry->name, NAMEDATALEN);
					StrNCpy(info->cursorName, entry->cursorName, NAMEDATALEN);
					info->state = entry->state;
					info->sessionId = entry->sessionID;
					info->userId = entry->userID;
					idx++;
				}
			}
		}
		LWLockRelease(ParallelCursorEndpointLock);

		/* return to original context when allocating transient memory */
		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	all_info = funcctx->user_fctx;

	while (all_info->cur_idx < all_info->total_num)
	{
		Datum				result;
		char				tokenStr[ENDPOINT_TOKEN_STR_LEN + 1];
		EndpointInfo	   *info = &all_info->infos[all_info->cur_idx++];
		GpSegConfigEntry   *segCnfInfo = dbid_get_dbinfo(info->dbid);

		MemSet(values, 0, sizeof(values));
		MemSet(nulls, 0, sizeof(nulls));

		values[0] = Int32GetDatum(info->dbid);
		endpoint_token_hex2str(info->token, tokenStr);
		values[1] = CStringGetTextDatum(tokenStr);
		values[2] = CStringGetTextDatum(info->cursorName);
		values[3] = Int32GetDatum(info->sessionId);
		values[4] = CStringGetTextDatum(segCnfInfo->hostname);
		values[5] = Int32GetDatum(segCnfInfo->port);
		values[6] = ObjectIdGetDatum(info->userId);
		values[7] = CStringGetTextDatum(state_enum_to_string(info->state));
		values[8] = CStringGetTextDatum(info->name);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		SRF_RETURN_NEXT(funcctx, result);
	}
	SRF_RETURN_DONE(funcctx);
}

/*
 * Display the status of all valid Endpoint of current
 * backend in shared memory.
 * If current user is superuser, list all endpoints on this segment.
 * Or only show current user's endpoints on this segment.
 */
Datum
gp_segment_endpoints(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	MemoryContext oldcontext;
	Datum		values[10];
	bool		nulls[10];
	HeapTuple	tuple;
	int			*endpoint_idx;

	if (SRF_IS_FIRSTCALL())
	{
		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/* switch to memory context appropriate for multiple function calls */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* build tuple descriptor */
		TupleDesc	tupdesc = CreateTemplateTupleDesc(10);

		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "auth_token", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "databaseid", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "senderpid", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "receiverpid", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "state", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "dbid", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "sessionid", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 8, "userid", OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 9, "endpointname", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 10, "cursorname", TEXTOID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		endpoint_idx = (int *) palloc0(sizeof(int));
		funcctx->user_fctx = (void *) endpoint_idx;

		/* return to original context when allocating transient memory */
		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	endpoint_idx = (int *) funcctx->user_fctx;

	LWLockAcquire(ParallelCursorEndpointLock, LW_SHARED);
	while (*endpoint_idx < MAX_ENDPOINT_SIZE)
	{
		Datum		result;
		const Endpoint entry = get_endpointdesc_by_index(*endpoint_idx);

		MemSet(values, 0, sizeof(values));
		MemSet(nulls, 0, sizeof(nulls));

		/*
		 * Only allow current user to list his/her own endpoints, or let
		 * superuser list all endpoints.
		 */
		if (!entry->empty && (superuser() || entry->userID == GetUserId()))
		{
			char	   *state = NULL;
			int8		token[ENDPOINT_TOKEN_HEX_LEN];
			char	    tokenStr[ENDPOINT_TOKEN_STR_LEN + 1];

			get_token_from_session_hashtable(entry->sessionID, entry->userID, token);
			endpoint_token_hex2str(token, tokenStr);
			values[0] = CStringGetTextDatum(tokenStr);
			values[1] = Int32GetDatum(entry->databaseID);
			values[2] = Int32GetDatum(entry->senderPid);
			values[3] = Int32GetDatum(entry->receiverPid);
			state = state_enum_to_string(entry->state);
			values[4] = CStringGetTextDatum(state);
			values[5] = Int32GetDatum(GpIdentity.dbid);
			values[6] = Int32GetDatum(entry->sessionID);
			values[7] = ObjectIdGetDatum(entry->userID);
			values[8] = CStringGetTextDatum(entry->name);
			values[9] = CStringGetTextDatum(entry->cursorName);

			tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
			result = HeapTupleGetDatum(tuple);
			(*endpoint_idx)++;
			LWLockRelease(ParallelCursorEndpointLock);
			SRF_RETURN_NEXT(funcctx, result);
		}
		else
			(*endpoint_idx)++;
	}
	LWLockRelease(ParallelCursorEndpointLock);
	SRF_RETURN_DONE(funcctx);
}

char *
state_enum_to_string(EndpointState state)
{
	char	   *result = NULL;

	switch (state)
	{
		case ENDPOINTSTATE_READY:
			result = STR_ENDPOINT_STATE_READY;
			break;
		case ENDPOINTSTATE_RETRIEVING:
			result = STR_ENDPOINT_STATE_RETRIEVING;
			break;
		case ENDPOINTSTATE_ATTACHED:
			result = STR_ENDPOINT_STATE_ATTACHED;
			break;
		case ENDPOINTSTATE_FINISHED:
			result = STR_ENDPOINT_STATE_FINISHED;
			break;
		case ENDPOINTSTATE_RELEASED:
			result = STR_ENDPOINT_STATE_RELEASED;
			break;
		case ENDPOINTSTATE_INVALID:

			/*
			 * This function is called when displays endpoint's information.
			 * Only valid endpoints will be printed out. So the state of the
			 * endpoint shouldn't be invalid.
			 */
			ereport(ERROR, (errmsg("invalid state of endpoint")));
			break;
	}
	Assert(result != NULL);
	return result;
}

static EndpointState
state_string_to_enum(const char *state)
{
	if (strcmp(state, STR_ENDPOINT_STATE_READY) == 0)
		return ENDPOINTSTATE_READY;
	else if (strcmp(state, STR_ENDPOINT_STATE_RETRIEVING) == 0)
		return ENDPOINTSTATE_RETRIEVING;
	else if (strcmp(state, STR_ENDPOINT_STATE_ATTACHED) == 0)
		return ENDPOINTSTATE_ATTACHED;
	else if (strcmp(state, STR_ENDPOINT_STATE_FINISHED) == 0)
		return ENDPOINTSTATE_FINISHED;
	else if (strcmp(state, STR_ENDPOINT_STATE_RELEASED) == 0)
		return ENDPOINTSTATE_RELEASED;
	else
	{
		ereport(ERROR, (errmsg("unknown endpoint state %s", state)));
		return ENDPOINTSTATE_INVALID;
	}
}

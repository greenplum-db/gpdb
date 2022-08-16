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
#include "utils/faultinjector.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbendpoint.h"
#include "cdbendpoint_private.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"


/*
 * Convert the string-format token to array
 * (e.g. "123456789ABCDEF0" to [1,2,3,4,5,6,7,8,9,A,B,C,D,E,F,0]).
 */
void
endpoint_token_str2arr(const char *tokenStr, int8 *token)
{
	if (strlen(tokenStr) == ENDPOINT_TOKEN_STR_LEN)
		hex_decode(tokenStr, ENDPOINT_TOKEN_STR_LEN, (char *) token);
	else
		ereport(FATAL, (errcode(ERRCODE_INVALID_PASSWORD),
				 errmsg("retrieve auth token is invalid")));
}

/*
 * Convert the array-format token to string
 * (e.g. [1,2,3,4,5,6,7,8,9,A,B,C,D,E,F,0] to "123456789ABCDEF0").
 */
void
endpoint_token_arr2str(const int8 *token, char *tokenStr)
{
	hex_encode((const char *) token, ENDPOINT_TOKEN_ARR_LEN, tokenStr);
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
	return memcmp(token1, token2, ENDPOINT_TOKEN_ARR_LEN) == 0;
}

bool
endpoint_name_equals(const char *name1, const char *name2)
{
	return strncmp(name1, name2, NAMEDATALEN) == 0;
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
		default:
			ereport(ERROR, (errmsg("unknown state of endpoint (%d)", state)));
			break;
	}
	Assert(result != NULL);
	return result;
}

/*
 * Generate the endpoint name.
 */
void
generate_endpoint_name(char *name, const char *cursorName)
{
	int			len,
				cursorLen;

	len = 0;

	/* part1: cursor name */
	cursorLen = strlen(cursorName);
	if (cursorLen > ENDPOINT_NAME_CURSOR_LEN)
		cursorLen = ENDPOINT_NAME_CURSOR_LEN;
	memcpy(name, cursorName, cursorLen);
	len += cursorLen;

	/* part2: gp_session_id */
	snprintf(name + len, ENDPOINT_NAME_SESSIONID_LEN + 1, "%08x", gp_session_id);
	len += ENDPOINT_NAME_SESSIONID_LEN;

	/*
	 * part3: gp_command_count In theory cursor name + gp_session_id is
	 * enough, but we'd keep this part to avoid confusion or potential issues
	 * for the scenario that in the same session (thus same gp_session_id),
	 * two endpoints with same cursor names (happens the cursor is
	 * dropped/rollbacked and then recreated) and retrieve the endpoints would
	 * be confusing for users that in the same retrieve connection.
	 */
	snprintf(name + len, ENDPOINT_NAME_COMMANDID_LEN + 1, "%08x", gp_command_count);
	len += ENDPOINT_NAME_COMMANDID_LEN;

	name[len] = '\0';
}
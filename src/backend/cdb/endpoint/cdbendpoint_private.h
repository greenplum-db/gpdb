/*-------------------------------------------------------------------------
 *
 * cdbendpoint_private.h
 *	  Internal routines for parallel retrieve cursor.
 *
 *
 * Copyright (c) 2020-Present VMware, Inc. or its affiliates
 *
 * src/backend/cdb/endpoints/cdbendpoint_private.h
 *
 *-------------------------------------------------------------------------
 */

#include "cdb/cdbendpoint.h"

#ifndef CDBENDPOINTINTERNAL_H
#define CDBENDPOINTINTERNAL_H

#define MAX_ENDPOINT_SIZE				1024
#define ENDPOINT_TOKEN_ARR_LEN			16
#define ENDPOINT_TOKEN_STR_LEN			(ENDPOINT_TOKEN_ARR_LEN<<1)
#define InvalidEndpointSessionId		(-1)	/* follows invalid
												 * gp_session_id */

#define ENDPOINT_KEY_TUPLE_DESC_LEN		1
#define ENDPOINT_KEY_TUPLE_DESC			2
#define ENDPOINT_KEY_TUPLE_QUEUE		3

#define ENDPOINT_MSG_QUEUE_MAGIC		0x1949100119980802U

/*
 * Naming rules for endpoint:
 * cursorname_sessionIdHex_segIndexHex
 */

/* ACK NOTICE MESSAGE FROM ENDPOINT QE/Entry DB to QD */
#define ENDPOINT_NAME_SESSIONID_LEN	8
#define ENDPOINT_NAME_COMMANDID_LEN 8
#define ENDPOINT_NAME_CURSOR_LEN   (NAMEDATALEN - 1 - ENDPOINT_NAME_SESSIONID_LEN - ENDPOINT_NAME_COMMANDID_LEN)

extern void check_parallel_retrieve_cursor_errors(EState *estate);

/* Endpoint shared memory utility functions in "cdbendpoint.c" */
extern Endpoint *get_endpointdesc_by_index(int index);
extern Endpoint *find_endpoint(const char *endpointName, int sessionID);
extern void get_token_from_session_hashtable(int sessionId, Oid userID, int8 *token /* out */ );
extern int	get_session_id_from_token(Oid userID, const int8 *token);

/* utility functions in "cdbendpointutilities.c" */
extern bool endpoint_token_hex_equals(const int8 *token1, const int8 *token2);
extern bool endpoint_name_equals(const char *name1, const char *name2);
extern void endpoint_token_str2arr(const char *tokenStr, int8 *token);
extern void endpoint_token_arr2str(const int8 *token, char *tokenStr);
extern char *state_enum_to_string(EndpointState state);

#endif							/* CDBENDPOINTINTERNAL_H */

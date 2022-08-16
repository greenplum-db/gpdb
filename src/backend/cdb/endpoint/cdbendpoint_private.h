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

#define ENDPOINT_KEY_TUPLE_DESC_LEN		1
#define ENDPOINT_KEY_TUPLE_DESC			2
#define ENDPOINT_KEY_TUPLE_QUEUE		3

#define ENDPOINT_MSG_QUEUE_MAGIC		0x1949100119980802U

/*
 * Naming rules for endpoint:
 * cursorname_sessionIdHex_segIndexHex
 */


#define ENDPOINT_NAME_SESSIONID_LEN	8
#define ENDPOINT_NAME_COMMANDID_LEN 8
#define ENDPOINT_NAME_CURSOR_LEN   (NAMEDATALEN - 1 - ENDPOINT_NAME_SESSIONID_LEN - ENDPOINT_NAME_COMMANDID_LEN)

/* Endpoint shared memory utility functions in "cdbendpoint.c" */
extern Endpoint *find_endpoint(const char *endpointName, int sessionID);
extern int	get_session_id_from_token(Oid userID, const int8 *token);

/* utility functions in "cdbendpointutilities.c" */
extern bool endpoint_token_hex_equals(const int8 *token1, const int8 *token2);
extern bool endpoint_name_equals(const char *name1, const char *name2);

#endif							/* CDBENDPOINTINTERNAL_H */

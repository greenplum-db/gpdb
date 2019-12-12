/*-------------------------------------------------------------------------
 *
 * info_gp_internal.h
 *
 * Greenplum specific logic for determining tablespace paths
 * for a given tablespace_oid.
 *
 * Copyright (c) 2019-Present Pivotal Software, Inc.
 */

#include "old_tablespace_file_contents.h"

typedef enum GetTablespacePathResponseCodes {
	GetTablespacePathResponse_NOT_FOUND_IN_FILE,
	GetTablespacePathResponse_FOUND_USER_DEFINED_TABLESPACE,
	GetTablespacePathResponse_FOUND_SYSTEM_TABLESPACE,
} GetTablespacePathResponseCodes;

typedef struct GetTablespacePathResponse {
	GetTablespacePathResponseCodes code;
	char *tablespace_path;
} GetTablespacePathResponse;

/*
 * Return the Tablespace OID specific tablespace path to an GDPB 5 tablespace
 */
GetTablespacePathResponse
gp_get_tablespace_path(OldTablespaceFileContents *contents, Oid tablespace_oid);
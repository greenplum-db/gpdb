/*-------------------------------------------------------------------------
 *
 * info_gp.c
 *
 * Copyright (c) 2019-Present Pivotal Software, Inc.
 */

#include "info_gp.h"

#define EMPTY_TABLESPACE_PATH NULL

static GetTablespacePathResponse
make_response(GetTablespacePathResponseCodes code, char *path)
{
	GetTablespacePathResponse response;
	response.code = code;
	response.tablespace_path = path;
	return response;
}

static GetTablespacePathResponse
missing_file(void)
{
	return make_response(
		GetTablespacePathResponse_MISSING_FILE,
		EMPTY_TABLESPACE_PATH);
}

static GetTablespacePathResponse
not_found_in_file(void)
{
	return make_response(
		GetTablespacePathResponse_NOT_FOUND_IN_FILE,
		EMPTY_TABLESPACE_PATH);
}

static GetTablespacePathResponse
found_in_file(char *tablespace_path, Oid tablespace_oid)
{
	return make_response(
		GetTablespacePathResponse_FOUND,
		psprintf("%s/%u",
			tablespace_path,
			tablespace_oid));
}

GetTablespacePathResponse
gp_get_tablespace_path(OldTablespaceFileContents *oldTablespaceFileContents, Oid tablespace_oid)
{
	if (oldTablespaceFileContents == NULL)
		return missing_file();

	char* tablespace_path = old_tablespace_file_get_tablespace_path_for_oid(
		oldTablespaceFileContents,
		tablespace_oid);

	if (tablespace_path == NULL)
		return not_found_in_file();

	return found_in_file(tablespace_path, tablespace_oid);
}


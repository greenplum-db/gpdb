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
system_tablespace(void)
{
	return make_response(
		GetTablespacePathResponse_FOUND_SYSTEM_TABLESPACE,
		EMPTY_TABLESPACE_PATH);
}

static GetTablespacePathResponse
found_in_file(char *tablespace_path, Oid tablespace_oid)
{
	return make_response(
		GetTablespacePathResponse_FOUND_USER_DEFINED_TABLESPACE,
		psprintf("%s/%u",
			tablespace_path,
			tablespace_oid));
}

GetTablespacePathResponse
gp_get_tablespace_path(OldTablespaceFileContents *oldTablespaceFileContents, Oid tablespace_oid)
{
	if (oldTablespaceFileContents == NULL)
		return missing_file();

	OldTablespaceRecord *record = old_tablespace_file_get_record(
		oldTablespaceFileContents,
		tablespace_oid);

	if (record == NULL)
		return not_found_in_file();

	if (!OldTablespaceRecord_GetIsUserDefinedTablespace(record))
		return system_tablespace();

	return found_in_file(
		OldTablespaceRecord_GetDirectoryPath(record),
		tablespace_oid);
}


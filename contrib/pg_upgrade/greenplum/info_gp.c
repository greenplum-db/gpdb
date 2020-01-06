/*-------------------------------------------------------------------------
 *
 * info_gp.c
 *
 * Copyright (c) 2019-Present Pivotal Software, Inc.
 */

#include "info_gp.h"
#include "info_gp_internal.h"
#include "old_tablespace_file_gp_internal.h"

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
found_in_file(char *tablespace_path)
{
	return make_response(
		GetTablespacePathResponse_FOUND_USER_DEFINED_TABLESPACE,
		tablespace_path);
}

GetTablespacePathResponse
gp_get_tablespace_path(OldTablespaceFileContents *oldTablespaceFileContents, Oid tablespace_oid)
{
	OldTablespaceRecord *record = old_tablespace_file_get_record(
		oldTablespaceFileContents,
		tablespace_oid);

	if (record == NULL)
		return not_found_in_file();

	if (!OldTablespaceRecord_GetIsUserDefinedTablespace(record))
		return system_tablespace();

	return found_in_file(
		OldTablespaceRecord_GetDirectoryPath(record));
}

/*
 * Determine if we need to look up the tablespace path in the old tablespace
 * file and do so. We only need to look in the old tablespaces file when the
 * gpdb version has filespaces and tablespaces.
 *
 * Look in the old tablespace file for the tablespace path of the given tablepace oid,
 * and return the result to the user.
 *
 * For system defined tablespaces, upgrade expects to use its own
 * spclocation information
 *
 * Upon a failure, raise an error to the user, as these are unexpected/exceptional
 * situations.
 *
 */
char *
determine_db_tablespace_path(ClusterInfo *currentCluster,
                             char *spclocation,
                             Oid tablespace_oid)
{
	if (currentCluster != &old_cluster ||
		!old_tablespace_file_contents_exists() ||
		!is_gpdb_version_with_filespaces(currentCluster))
		return spclocation;

	GetTablespacePathResponse response = gp_get_tablespace_path(
		get_old_tablespace_file_contents(),
		tablespace_oid);

	switch (response.code)
	{
		case GetTablespacePathResponse_FOUND_USER_DEFINED_TABLESPACE:
			return pg_strdup(response.tablespace_path);
		case GetTablespacePathResponse_FOUND_SYSTEM_TABLESPACE:
			return spclocation;
		case GetTablespacePathResponse_NOT_FOUND_IN_FILE:
			pg_fatal("expected the old tablespace file to "
			         "contain a tablespace entry for tablespace oid = %u\n",
			         tablespace_oid);
		default:
			pg_fatal("unknown get tablespace path response\n");
	}
}


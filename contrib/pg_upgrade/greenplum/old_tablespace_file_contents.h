/*-------------------------------------------------------------------------
 *
 * old_tablespace_file_contents.h
 *
 * Data type to hold Greenplum 5's filespace and tablespace information
 * to be used by upgrade.
 *
 * Copyright (c) 2019-Present Pivotal Software, Inc.
 */

#ifndef OLD_TABLESPACE_FILE_CONTENTS_H
#define OLD_TABLESPACE_FILE_CONTENTS_H

#include "postgres_ext.h"
#include "postgres_fe.h"

typedef struct OldTablespaceFileContentsData OldTablespaceFileContents;
typedef struct OldTablespaceRecordData OldTablespaceRecord;

int
OldTablespaceFileContents_TotalNumberOfTablespaces(OldTablespaceFileContents *contents);

char **
OldTablespaceFileContents_GetArrayOfTablespacePaths(OldTablespaceFileContents *contents);

/*
 * Return an OldTablespaceFileContents containing tablespaces in the given csv
 * file.
 *
 * File contents expected to contain rows with the structure:
 *
 * "dbid","tablespace oid","tablespace path"
 *
 */
OldTablespaceFileContents *
parse_old_tablespace_file_contents(const char *file_path);

/*
 * Return an OldTablespaceFileContents containing only tablespaces for the 
 * given dbid
 */
OldTablespaceFileContents *
filter_old_tablespace_file_for_dbid(OldTablespaceFileContents *contents, int dbid);

OldTablespaceRecord *
OldTablespaceFileContents_GetTablespaceRecord(
	OldTablespaceFileContents *contents,
	int dbid,
	char *tablespace_name);

OldTablespaceRecord **
OldTablespaceFileContents_GetTablespaceRecords(OldTablespaceFileContents *contents);

char *
OldTablespaceRecord_GetTablespaceName(OldTablespaceRecord *record);

char *
OldTablespaceRecord_GetDirectoryPath(OldTablespaceRecord *record);

Oid
OldTablespaceRecord_GetOid(OldTablespaceRecord *record);

bool
OldTablespaceRecord_GetIsUserDefinedTablespace(OldTablespaceRecord *record);

/*
 * free memory allocated for OldTablespaceFileContents
 */
void clear_old_tablespace_file_contents(OldTablespaceFileContents *contents);

bool is_old_tablespaces_file_empty(OldTablespaceFileContents *contents);

/*
 * Get the file path for a given old tablespace for the given tablespace oid
 */
OldTablespaceRecord *old_tablespace_file_get_record(
	OldTablespaceFileContents *contents, Oid oid);

#endif /* OLD_TABLESPACE_FILE_CONTENTS_H */

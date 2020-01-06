/*-------------------------------------------------------------------------
 *
 * old_tablespace_file_contents.c
 *
 * Copyright (c) 2019-Present Pivotal Software, Inc.
 *
 */
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>


#include "old_tablespace_file_contents.h"
#include "old_tablespace_file_parser.h"

struct OldTablespaceRecordData {
	char *tablespace_path;
	Oid tablespace_oid;
	char *tablespace_name;
	int dbid;
	bool is_user_defined;
};

struct OldTablespaceFileContentsData {
	int number_of_tablespaces;
	OldTablespaceRecord **old_tablespace_records;
};

static int
number_of_tablespaces_matching_dbid(OldTablespaceFileContents *originalContents, int dbid_to_find)
{
	int number_of_tablespaces = 0;

	for(int i = 0; i < originalContents->number_of_tablespaces; i++)
		if (originalContents->old_tablespace_records[i]->dbid == dbid_to_find)
			number_of_tablespaces++;

	return number_of_tablespaces;
}

static bool
matches_dbid(OldTablespaceRecord *record, int dbid)
{
	return record->dbid == dbid;
}

static void
copy_tablespace_record(OldTablespaceRecord *originalRecord, OldTablespaceRecord *newRecord)
{
	newRecord->dbid            = originalRecord->dbid;
	newRecord->tablespace_oid  = originalRecord->tablespace_oid;
	newRecord->tablespace_name = originalRecord->tablespace_name;
	newRecord->tablespace_path = originalRecord->tablespace_path;
	newRecord->is_user_defined = originalRecord->is_user_defined;
}

static void
populate_record_from_csv(OldTablespaceRecord *record, OldTablespaceFileParser_Document *document, int row_index)
{
	record->dbid =
		OldTablespaceFileParser_get_field_as_int(document, row_index, 0);
	record->tablespace_oid =
		OldTablespaceFileParser_get_field_as_oid(document, row_index, 1);
	record->tablespace_name = pg_strdup(
		OldTablespaceFileParser_get_field_as_string(document, row_index, 2));
	record->tablespace_path = pg_strdup(
		OldTablespaceFileParser_get_field_as_string(document, row_index, 3));
	record->is_user_defined =
		OldTablespaceFileParser_get_field_as_int(document, row_index, 4) != 0;
}

static void
initialize_old_tablespace_records(OldTablespaceFileContents *contents, int number_of_tablespaces)
{
	contents->old_tablespace_records = palloc0(sizeof(OldTablespaceRecord *) * number_of_tablespaces);

	for (int i = 0; i < number_of_tablespaces; i++)
		contents->old_tablespace_records[i] = palloc0(sizeof(OldTablespaceRecord));
}

static OldTablespaceFileContents *
make_old_tablespace_file_contents(int number_of_tablespaces)
{
	OldTablespaceFileContents *contents = palloc0(sizeof(OldTablespaceFileContents));
	contents->number_of_tablespaces = number_of_tablespaces;
	initialize_old_tablespace_records(contents, number_of_tablespaces);

	return contents;
}

static OldTablespaceFileParser_Document *
get_parser_document(const char *const file_path)
{
	FILE *file = fopen(file_path, "r");

	OldTablespaceFileParser_Document *document =
		                                 OldTablespaceFileParser_parse_file(file);

	if (file != NULL)
		fclose(file);

	return document;
}

void
clear_old_tablespace_file_contents(OldTablespaceFileContents *contents)
{
	for (int i = 0; i < contents->number_of_tablespaces; i++)
		pfree(contents->old_tablespace_records[i]);

	pfree(contents->old_tablespace_records);
	pfree(contents);
}

int
OldTablespaceFileContents_TotalNumberOfTablespaces(OldTablespaceFileContents *contents)
{
	return contents->number_of_tablespaces;
}

char **
OldTablespaceFileContents_GetArrayOfTablespacePaths(OldTablespaceFileContents *contents)
{
	char ** tablespace_paths = (char **) pg_malloc(contents->number_of_tablespaces * sizeof(char *));

	for (int i = 0; i < contents->number_of_tablespaces; i++)
		tablespace_paths[i] = pg_strdup(contents->old_tablespace_records[i]->tablespace_path);

	return tablespace_paths;
}

OldTablespaceRecord *
OldTablespaceFileContents_GetTablespaceRecord(OldTablespaceFileContents *contents, int dbid, char *tablespace_name)
{
	for (int i = 0; i < OldTablespaceFileContents_TotalNumberOfTablespaces(contents); i++)
	{
		OldTablespaceRecord *current_record = contents->old_tablespace_records[i];

		if (strcmp(current_record->tablespace_name, tablespace_name) == 0 && current_record->dbid == dbid)
			return current_record;
	}

	return NULL;
}

OldTablespaceFileContents *
parse_old_tablespace_file_contents(const char *const file_path)
{
	OldTablespaceFileParser_Document *document = get_parser_document(file_path);

	OldTablespaceFileContents *contents = make_old_tablespace_file_contents(
		OldTablespaceFileParser_number_of_rows(document));

	for (int i = 0; i < contents->number_of_tablespaces; i++)
		populate_record_from_csv(contents->old_tablespace_records[i], document, i);

	OldTablespaceFileParser_clear_document(document);

	return contents;
}

OldTablespaceFileContents*
filter_old_tablespace_file_for_dbid(OldTablespaceFileContents *originalContents, int dbid_to_find)
{
	int match_index = 0;

	OldTablespaceFileContents *result = make_old_tablespace_file_contents(
		number_of_tablespaces_matching_dbid(originalContents, dbid_to_find));

	for(int i = 0; i < originalContents->number_of_tablespaces; i++)
	{
		OldTablespaceRecord *originalRecord = originalContents->old_tablespace_records[i];
		OldTablespaceRecord *newRecord = result->old_tablespace_records[match_index];

		if (matches_dbid(originalRecord, dbid_to_find))
		{
			match_index++;
			copy_tablespace_record(originalRecord, newRecord);
		}
	}

	return result;
}

OldTablespaceRecord *
old_tablespace_file_get_record(OldTablespaceFileContents *contents, Oid tablespace_oid)
{
	OldTablespaceRecord *currentRecord;

	for (int i = 0; i < contents->number_of_tablespaces; i++)
	{
		currentRecord = contents->old_tablespace_records[i];

		if (currentRecord->tablespace_oid == tablespace_oid)
			return currentRecord;
	}

	return NULL;
}

OldTablespaceRecord **
OldTablespaceFileContents_GetTablespaceRecords(OldTablespaceFileContents *contents)
{
	return contents->old_tablespace_records;
}

char *
OldTablespaceRecord_GetTablespaceName(OldTablespaceRecord *record)
{
	return record->tablespace_name;
}

char *
OldTablespaceRecord_GetDirectoryPath(OldTablespaceRecord *record)
{
	return record->tablespace_path;
}

bool
OldTablespaceRecord_GetIsUserDefinedTablespace(OldTablespaceRecord *record)
{
	return record->is_user_defined;
}

Oid
OldTablespaceRecord_GetOid(OldTablespaceRecord *record)
{
	return record->tablespace_oid;
}

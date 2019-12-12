/*-------------------------------------------------------------------------
 *
 * old_tablespace_file_parser.h
 *
 * Responsible for reading a file and being able to extract data from it
 * for use when populating OldTablespaceFileContents.
 *
 * Copyright (c) 2019-Present Pivotal Software, Inc.
 *
 */
#include <stdio.h>
#include "postgres_ext.h"

typedef struct OldTablespaceFileParser_DocumentData OldTablespaceFileParser_Document;

/*
 * Read through file and present contents as rows:
 * 
 * File is expected to have rows structured as
 * 
 * [gp dbid],[tablespace oid],[tablespace name],[tablespace path]
 */
OldTablespaceFileParser_Document *OldTablespaceFileParser_parse_file(FILE *file);

/*
 * free memory allocated by OldTablespaceFileParser_parse_file
 */
void OldTablespaceFileParser_clear_document(OldTablespaceFileParser_Document *document);

/*
 * Return the number of records in the document
 */
int OldTablespaceFileParser_number_of_rows(OldTablespaceFileParser_Document *document);


char *OldTablespaceFileParser_get_field_as_string(OldTablespaceFileParser_Document* document, int row_number, int field_index);
int OldTablespaceFileParser_get_field_as_int(OldTablespaceFileParser_Document* document, int row_index, int field_index);
Oid OldTablespaceFileParser_get_field_as_oid(OldTablespaceFileParser_Document *document, int row_index, int field_index);

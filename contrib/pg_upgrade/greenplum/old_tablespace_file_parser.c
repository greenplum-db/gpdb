/*-------------------------------------------------------------------------
 *
 * old_tablespace_file_parser.c
 *
 * Copyright (c) 2019-Present Pivotal Software, Inc.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "old_tablespace_file_parser.h"
#include "old_tablespace_file_parser_observer.h"
#include "postgres_fe.h"

#define atooid(x)  ((Oid) strtoul((x), NULL, 10))
#define MAX_ROW_LENGTH 2000
#define MAX_NUMBER_OF_COLUMNS 10

typedef struct OldTablespaceFileParser_Row {
	int number_of_cells;
	char **cells;
} OldTablespaceFileParser_Row;

struct OldTablespaceFileParser_DocumentData
{
	int                         number_of_rows;
	OldTablespaceFileParser_Row **rows;
};

static void
parse_row(int current_row_index, OldTablespaceFileParser_Document *document, char row[MAX_ROW_LENGTH])
{
	char *tokens[MAX_NUMBER_OF_COLUMNS] = {0};
	int  number_of_tokens = 0;

	char *token = NULL;
	char *token_position;
	char *newline_position;

	char *row_text = pg_strdup(row);
	token = strtok_r(row_text, ",", &token_position);

	while (token != NULL && number_of_tokens < MAX_NUMBER_OF_COLUMNS)
	{
		tokens[number_of_tokens] = strtok_r(token, "\n", &newline_position);
		number_of_tokens++;
		token = strtok_r(NULL, ",", &token_position);
	}

	OldTablespaceFileParser_Row *parser_row = palloc0(sizeof(OldTablespaceFileParser_Row));
	parser_row->number_of_cells = number_of_tokens;
	parser_row->cells = palloc0(sizeof(char*) * number_of_tokens);

	for (int i = 0; i < number_of_tokens; i++)
		parser_row->cells[i] = pg_strdup(tokens[i]);

	document->rows[current_row_index] = parser_row;

	pfree(row_text);
}

static void
free_parser_row(OldTablespaceFileParser_Row *row)
{
	for (int i = 0; i < row->number_of_cells; i++)
		pfree(row->cells[i]);

	pfree(row);
}

static OldTablespaceFileParser_Document *
make_document(int number_of_rows)
{
	OldTablespaceFileParser_Document *document = palloc0(sizeof(OldTablespaceFileParser_Document));
	document->number_of_rows = number_of_rows;
	document->rows = palloc0(sizeof(document->rows) * document->number_of_rows);
	return document;
}

/*
 * OldTablespaceFileParser_parse_file
 *
 * expects file to have the fields without a header:
 *
 * [dbid],[tablespace oid],[tablespace name],[path],[is user defined tablespace (0 or 1)]
 *
 */
OldTablespaceFileParser_Document *
OldTablespaceFileParser_parse_file(FILE *file)
{
	int number_of_rows = 0;
	int current_row_index = 0;
	char contents[MAX_ROW_LENGTH];

	if (file == NULL)
		return make_document(0);

	/*
	 * determine number of rows
	 */
	while ((fgets(contents, MAX_ROW_LENGTH, file)) != NULL)
		number_of_rows++;

	/*
	 * reset the for reading again
	 */
	rewind(file);

	/*
	 * initialize document
	 */
	OldTablespaceFileParser_Document *document = make_document(number_of_rows);

	/*
	 * populate document
	 */
	char *row_contents = palloc0(sizeof(char) * MAX_ROW_LENGTH);
	while ((fgets(row_contents, MAX_ROW_LENGTH, file)) != NULL)
		parse_row(current_row_index++, document, row_contents);

	return document;
}

char *
OldTablespaceFileParser_get_field_as_string(OldTablespaceFileParser_Document *document, int row_index, int field_index)
{
	if (row_index >= document->number_of_rows)
	{
		OldTablespaceFileParser_invalid_access_error_for_row(row_index);
		return NULL;
	}

	OldTablespaceFileParser_Row *row = document->rows[row_index];

	if (field_index >= row->number_of_cells)
	{
		OldTablespaceFileParser_invalid_access_error_for_field(row_index, field_index);
		return NULL;
	}

	return row->cells[field_index];
}

int
OldTablespaceFileParser_number_of_rows(OldTablespaceFileParser_Document *document)
{
	return document->number_of_rows;
}

int
OldTablespaceFileParser_get_field_as_int(OldTablespaceFileParser_Document *document, int row_index, int field_index)
{
	char *field = OldTablespaceFileParser_get_field_as_string(document,
	                                            row_index,
	                                            field_index);

	if (field == NULL) return -1;

	return atoi(field);
}

Oid
OldTablespaceFileParser_get_field_as_oid(OldTablespaceFileParser_Document *document, int row_index, int field_index)
{
	char *field = OldTablespaceFileParser_get_field_as_string(document,
	                                            row_index,
	                                            field_index);
	if (field == NULL) return InvalidOid;

	return atooid(field);
}

void
OldTablespaceFileParser_clear_document(OldTablespaceFileParser_Document *document)
{
	for (int i = 0; i < document->number_of_rows; i++)
		free_parser_row(document->rows[i]);

	pfree(document);
}

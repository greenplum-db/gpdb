#include "pg_upgrade.h"
#include "old_tablespace_file_parser.h"

void
OldTablespaceFileParser_invalid_access_error_for_row(int invalid_row_index)
{
	pg_fatal("attempted to read an invalid row from an old tablespace file. row index %d",
	         invalid_row_index);
}

void
OldTablespaceFileParser_invalid_access_error_for_field(int invalid_row_index, int invalid_field_index)
{
	pg_fatal("attempted to read an invalid field from an old tablespace file. row index %d, field index %d",
	         invalid_row_index, invalid_field_index);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utilities/test-upgrade-helpers.h"
#include "greenplum/old_tablespace_file_parser_observer.h"

void
OldTablespaceFileParser_invalid_access_error_for_field(int invalid_row_index, int invalid_field_index)
{
	printf("attempted to access invalid field: {row_index=%d, field_index=%d}",
	       invalid_row_index,
	       invalid_field_index);

	exit(1);
}

void
OldTablespaceFileParser_invalid_access_error_for_row(int invalid_row_index)
{
	printf("attempted to access invalid row: {row_index=%d}",
	       invalid_row_index);

	exit(1);
}

int
main(int argc, char *argv[])
{
	performUpgrade();
}

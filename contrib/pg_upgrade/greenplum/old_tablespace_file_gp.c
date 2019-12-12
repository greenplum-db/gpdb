/*-------------------------------------------------------------------------
 *
 * old_tablespace_file_gp.c
 *
 * Definitions of functions to manage the interface between pg_upgrade and
 * greenplum's storage of the old_tablespace_file_contents.
 *
 * Copyright (c) 2019-Present Pivotal Software, Inc.
 *
 */

#include "old_tablespace_file_contents.h"
#include "old_tablespace_file_gp.h"
#include "old_tablespace_file_gp_internal.h"

static OldTablespaceFileContents *old_tablespace_file_contents;

OldTablespaceFileContents *
get_old_tablespace_file_contents(void)
{
	return old_tablespace_file_contents;
}

void
set_old_tablespace_file_contents(
	OldTablespaceFileContents *new_old_tablespace_file_contents)
{
	old_tablespace_file_contents = new_old_tablespace_file_contents;
}

bool
old_tablespace_file_contents_exists(void)
{
	return get_old_tablespace_file_contents() != NULL;
}

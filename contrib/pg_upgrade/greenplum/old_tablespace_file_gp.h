/*-------------------------------------------------------------------------
 *
 * old_tablespace_file_gp.h
 *
 * Functions to manage the interface between pg_upgrade and greenplum's
 * storage of the old_tablespace_file_contents.
 *
 * Copyright (c) 2019-Present Pivotal Software, Inc.
 *
 */
#include "old_tablespace_file_contents.h"

bool old_tablespace_file_contents_exists(void);
OldTablespaceFileContents *get_old_tablespace_file_contents(void);
void set_old_tablespace_file_contents(OldTablespaceFileContents *old_tablespace_file_contents);

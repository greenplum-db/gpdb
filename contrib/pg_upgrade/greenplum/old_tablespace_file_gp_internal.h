/*-------------------------------------------------------------------------
 *
 * old_tablespace_file_gp_internal.h
 *
 * Copyright (c) 2019-Present Pivotal Software, Inc.
 *
 */
#include "old_tablespace_file_contents.h"

OldTablespaceFileContents *get_old_tablespace_file_contents(void);
void set_old_tablespace_file_contents(OldTablespaceFileContents *old_tablespace_file_contents);

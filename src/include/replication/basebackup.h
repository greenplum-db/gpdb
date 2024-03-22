/*-------------------------------------------------------------------------
 *
 * basebackup.h
 *	  Exports from replication/basebackup.c.
 *
 * Portions Copyright (c) 2010-2019, PostgreSQL Global Development Group
 *
 * src/include/replication/basebackup.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef _BASEBACKUP_H
#define _BASEBACKUP_H

#include "nodes/replnodes.h"

/*
 * Minimum and maximum values of MAX_RATE option in BASE_BACKUP command.
 */
#define MAX_RATE_LOWER	32
#define MAX_RATE_UPPER	1048576


typedef struct
{
	char	   *oid;
	char	   *path;
	char	   *rpath;			/* relative path within PGDATA, or NULL */
	int64		size;
} tablespaceinfo;

extern void SendBaseBackup(BaseBackupCmd *cmd);

extern int64 sendTablespace(char *path, bool sizeonly);

typedef char* (*basebackup_file_before_verify_hook_type)(char *buf, const char *fname, int blkno, size_t size);
extern PGDLLIMPORT basebackup_file_before_verify_hook_type basebackup_file_before_verify_hook;

#endif							/* _BASEBACKUP_H */

/*-------------------------------------------------------------------------
 *
 * cdb_lockbox.h
 *	  Definitions for cdb_lockbox.c
 *
 * Portions Copyright (c) 2016-Present Pivotal Software, Inc.
 *
 * src/bin/pg_dump/cdb/cdb_lockbox.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef CDB_LOCKBOX_H
#define CDB_LOCKBOX_H

typedef struct
{
	char	   *key;
	char	   *value;
} lockbox_item;

/*
 * A list of key-value pairs, used to hold the contents of lockbox file
 * in memory.
 */
typedef struct
{
	int			nitems;
	lockbox_item *items;
} lockbox_content;

extern int lb_store(const char *filepath, const lockbox_content *content);
extern lockbox_content *lb_load(const char *filepath);
extern char *lb_get_item(lockbox_content *content, const char *key);
extern char *lb_get_item_or_error(lockbox_content *content, const char *key,
					 const char *filepath);

extern char *lb_obfuscate(const char *input);
extern char *lb_deobfuscate(const char *input);

#endif   /* CDB_LOCKBOX_H */

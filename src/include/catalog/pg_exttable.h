/*-------------------------------------------------------------------------
 *
 * pg_exttable.h
 *	  definitions for system wide external relations
 *
 * Portions Copyright (c) 2007-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/pg_exttable.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef PG_EXTTABLE_H
#define PG_EXTTABLE_H

#include "catalog/genbki.h"
#include "nodes/pg_list.h"

/*
 * Descriptor of a single external relation.
 * For now very similar to the catalog row itself but may change in time.
 */
typedef struct ExtTableEntry
{
	List*	urilocations;
	List*	execlocations;
	char	fmtcode;
	List*	options;
	char*	command;
	int		rejectlimit;
	char	rejectlimittype;
	char	logerrors;
    int		encoding;
    bool	iswritable;
    bool	isweb;		/* extra state, not cataloged */
} ExtTableEntry;

extern List * tokenizeLocationUris(char *locations);

extern ExtTableEntry *GetExtTableEntry(Oid relid);

extern ExtTableEntry *GetExtTableEntryIfExists(Oid relid);

extern ExtTableEntry *GetExtFromForeignTableOptions(List *ftoptons, Oid relid);

#define fmttype_is_custom(c) (c == 'b')
#define fmttype_is_text(c)   (c == 't')
#define fmttype_is_csv(c)    (c == 'c')

#endif /* PG_EXTTABLE_H */

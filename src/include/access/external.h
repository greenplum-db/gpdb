/*-------------------------------------------------------------------------
 *
 * external.h
 *	  routines for getting external info from external table foreign table.
 *
 * Portions Copyright (c) 2020-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/access/external.h
*
*-------------------------------------------------------------------------
*/
#ifndef EXTERNAL_H
#define EXTERNAL_H

#include "nodes/pg_list.h"
#include "nodes/plannodes.h"

#define fmttype_is_custom(c) (c == 'b')
#define fmttype_is_text(c)   (c == 't')
#define fmttype_is_csv(c)    (c == 'c')

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

extern List * TokenizeLocationUris(char *locations);

extern ExtTableEntry *GetExtTableEntry(Oid relid);
extern ExtTableEntry *GetExtTableEntryIfExists(Oid relid);
extern ExtTableEntry *GetExtFromForeignTableOptions(List *ftoptons, Oid relid);

extern ExternalScanInfo *MakeExternalScanInfo(ExtTableEntry *extEntry);
extern ForeignScan *BuildForeignScanForExternalTable(Oid relid, Index scanrelid, List *qual, List *targetlist);


#endif   /* EXTERNAL_H */
/*-------------------------------------------------------------------------
 *
 * pg_appendonly_fn.h
 *	  Functions related to AO-table related system catalogs.
 *
 * Portions Copyright (c) 2008-2010, Greenplum Inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/pg_appendonly_fn.h
 *
 *
 * GPDB_12_MERGE_FIXME: In upstream commit cefa387153, all *_fn.h header
 * files were merged back to the plan *.h files. Should do the same here.
 *-------------------------------------------------------------------------
 */
#ifndef PG_APPENDONLY_FN_H
#define PG_APPENDONLY_FN_H

#include "catalog/genbki.h"
#include "utils/relcache.h"
#include "utils/snapshot.h"

extern void
InsertAppendOnlyEntry(Oid relid,
					  int blocksize,
					  int safefswritesize,
					  int compresslevel,
					  bool checksum,
                      bool columnstore,
					  char* compresstype,
					  Oid segrelid,
					  Oid blkdirrelid,
					  Oid blkdiridxid,
					  Oid visimaprelid,
					  Oid visimapidxid);

void
GetAppendOnlyEntryAttributes(Oid relid,
							 int32 *blocksize,
							 int32 *safefswritesize,
							 int16 *compresslevel,
							 bool *checksum,
							 NameData *compresstype);

/*
 * Get the OIDs of the auxiliary relations and their indexes for an appendonly
 * relation.
 *
 * The OIDs will be retrieved only when the corresponding output variable is
 * not NULL.
 */
void
GetAppendOnlyEntryAuxOids(Oid relid,
						  Snapshot appendOnlyMetaDataSnapshot,
						  Oid *segrelid,
						  Oid *blkdirrelid,
						  Oid *blkdiridxid,
						  Oid *visimaprelid,
						  Oid *visimapidxid);

/*
 * Update the segrelid and/or blkdirrelid if the input new values
 * are valid OIDs.
 */
extern void
UpdateAppendOnlyEntryAuxOids(Oid relid,
							 Oid newSegrelid,
							 Oid newBlkdirrelid,
							 Oid newBlkdiridxid,
							 Oid newVisimaprelid,
							 Oid newVisimapidxid);

extern void
RemoveAppendonlyEntry(Oid relid);

extern void
SwapAppendonlyEntries(Oid entryRelId1, Oid entryRelId2);

#endif   /* PG_APPENDONLY_FN_H */

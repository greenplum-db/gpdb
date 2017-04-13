/*-------------------------------------------------------------------------
 *
 * appendonlywriter.h
 *	  Definitions for appendonlywriter.c
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/include/access/appendonlywriter.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef APPENDONLYWRITER_H
#define APPENDONLYWRITER_H

#include "access/aosegfiles.h"
#include "nodes/relation.h"

/*
 * Maximum concurrent number of writes into a single append only table.
 * TODO: may want to make this a guc instead (can only be set at gpinit time).
 */
#define MAX_AOREL_CONCURRENCY 128

/*
 * functions in appendonlywriter.c
 */
extern int  SetSegnoForWrite(Relation rel, List *avoidsegnos);
extern int  SetSegnoForCompaction(Relation rel, List *avoidsegnos);
extern void AORelIncrementModCount(Relation parentrel);

extern void ValidateAppendOnlyMetaDataSnapshot(
								   Snapshot *appendOnlyMetaDataSnapshot);

#endif							/* APPENDONLYWRITER_H */

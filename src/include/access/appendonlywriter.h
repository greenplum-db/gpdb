/*-------------------------------------------------------------------------
 *
 * appendonlywriter.h
 *	  Definitions for appendonlywriter.c
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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

/*
 * Maximum concurrent number of writes into a single append only table.
 * TODO: may want to make this a guc instead (can only be set at gpinit time).
 */
#define MAX_AOREL_CONCURRENCY 128

/*
 * Segfile 0 is reserved for special case write operations that don't need
 * to worry about concurrency. Currently these are:
 *
 *   - Table rewrites that are sometimes done as a part of ALTER TABLE.
 *   - CTAS, which runs on an exclusively locked destination table.
 *
 * There's no particular reason those operations couldn't use the normal
 * routines for finding a target segment, but this is historical behavior.
 * Similarly, the normal routines don't choose segfile 0, but there's no
 * particular reason reason it couldn't be used for normal operations.
 */
#define RESERVED_SEGNO 0

/*
 * functions in appendonlywriter.c
 */
extern void LockSegnoForWrite(Relation rel, int segno);
extern int  ChooseSegnoForWrite(Relation rel);
extern int  ChooseSegnoForCompactionWrite(Relation rel, List *avoid_segnos);
extern int  ChooseSegnoForCompaction(Relation rel, List *avoidsegnos);
extern void AORelIncrementModCount(Relation parentrel);

#endif							/* APPENDONLYWRITER_H */

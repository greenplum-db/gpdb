/*--------------------------------------------------------------------------
 * gin.h
 *	  Public header file for Generalized Inverted Index access method.
 *
 *	Copyright (c) 2006-2011, PostgreSQL Global Development Group
 *
 *	src/include/access/gin.h
 *--------------------------------------------------------------------------
 */
#ifndef GIN_H
#define GIN_H

#include "access/relscan.h"
#include "access/sdir.h"
#include "access/xlogdefs.h"
#include "storage/bufpage.h"
#include "storage/off.h"
#include "utils/rel.h"
#include "access/genam.h"
#include "access/itup.h"
#include "access/xlog.h"
#include "storage/block.h"
#include "utils/relcache.h"


/*
 * amproc indexes for inverted indexes.
 */
#define GIN_COMPARE_PROC			   1
#define GIN_EXTRACTVALUE_PROC		   2
#define GIN_EXTRACTQUERY_PROC		   3
#define GIN_CONSISTENT_PROC			   4
#define GIN_COMPARE_PARTIAL_PROC	   5
#define GINNProcs					   5

/*
 * searchMode settings for extractQueryFn.
 */
#define GIN_SEARCH_MODE_DEFAULT			0
#define GIN_SEARCH_MODE_INCLUDE_EMPTY	1
#define GIN_SEARCH_MODE_ALL				2
#define GIN_SEARCH_MODE_EVERYTHING		3		/* for internal use only */

/*
 * GinStatsData represents stats data for planner use
 */
typedef struct GinStatsData
{
	BlockNumber nPendingPages;
	BlockNumber nTotalPages;
	BlockNumber nEntryPages;
	BlockNumber nDataPages;
	int64		nEntries;
	int32		ginVersion;
} GinStatsData;

/* GUC parameter */
extern PGDLLIMPORT int GinFuzzySearchLimit;

/* ginutil.c */
extern void ginGetStats(Relation index, GinStatsData *stats);
extern void ginUpdateStats(Relation index, const GinStatsData *stats);

/* ginxlog.c */
extern void gin_redo(XLogRecPtr beginLoc, XLogRecPtr lsn, XLogRecord *record);
extern void gin_desc(StringInfo buf, XLogRecord *record);
extern void gin_xlog_startup(void);
extern void gin_xlog_cleanup(void);
extern bool gin_safe_restartpoint(void);

extern void gin_mask(char *pagedata, BlockNumber blkno);

#endif   /* GIN_H */

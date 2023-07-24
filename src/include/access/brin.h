/*
 * AM-callable functions for BRIN indexes
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *		src/include/access/brin.h
 */
#ifndef BRIN_H
#define BRIN_H

#include "fmgr.h"
#include "nodes/execnodes.h"
#include "utils/relcache.h"


/*
 * Storage type for BRIN's reloptions
 */
typedef struct BrinOptions
{
	int32		vl_len_;		/* varlena header (do not touch directly!) */
	BlockNumber pagesPerRange;
	bool		autosummarize;
} BrinOptions;


/*
 * BrinStatsData represents stats data for planner use
 */
typedef struct BrinStatsData
{
	BlockNumber pagesPerRange;
	BlockNumber revmapNumPages;
} BrinStatsData;

/*
 * GPDB: We have different defaults for pages_per_range according to the type of
 * the base relation. Please see brin/README for more details.
 */
#define BRIN_DEFAULT_PAGES_PER_RANGE		32
#define BRIN_DEFAULT_PAGES_PER_RANGE_AO 	1
#define BRIN_UNDEFINED_PAGES_PER_RANGE 		0

/*
 * GPDB: We use a sentinel value of BRIN_UNDEFINED_PAGES_PER_RANGE to indicate
 * that the default assigned in the relcache is AM agnostic. The actual default
 * will be determined later in BrinGetPagesPerRange().
 */
#define BrinIsPagesPerRangeDefined(rd_options) \
	(rd_options && ((BrinOptions *) (rd_options))->pagesPerRange != BRIN_UNDEFINED_PAGES_PER_RANGE)

#define BrinDefaultPagesPerRange(isAO) \
	((isAO) ? BRIN_DEFAULT_PAGES_PER_RANGE_AO : BRIN_DEFAULT_PAGES_PER_RANGE)

#define BrinGetPagesPerRange(relation, isAO) \
	(BrinIsPagesPerRangeDefined((relation)->rd_options) ? \
	 ((BrinOptions *) (relation)->rd_options)->pagesPerRange : \
	  (BrinDefaultPagesPerRange((isAO))))
#define BrinGetAutoSummarize(relation) \
	((relation)->rd_options ? \
	 ((BrinOptions *) (relation)->rd_options)->autosummarize : \
	  false)

extern void brinGetStats(Relation index, BrinStatsData *stats);

#endif							/* BRIN_H */

/*-------------------------------------------------------------------------
 *
 * cdbpublic.c
 *
 *		Greenplum specific structures and typedefs exposed to the
 *      outside world.
 *
 *      This is a non-ideal solution, consider alternatives before
 *      adding to this file.
 *
 * Copyright (c) 2008-2008, Greenplum inc.
 *
 *-------------------------------------------------------------------------
 */

/* This file should not include any other files under "cdb" */

#ifndef CDBPUBLIC_H
#define CDBPUBLIC_H

#include "c.h"         /* DistributedTransactionId */

/* Things defined in this header */
typedef struct TMGXACT_LOG           TMGXACT_LOG;
typedef struct CdbExplain_Agg        CdbExplain_Agg;
typedef struct CdbCellBuf            CdbCellBuf;

/* From "cdb/cdbtm.h" */
struct TMGXACT_LOG
{
	char						gid[TMGIDSIZE];
	DistributedTransactionId	gxid;
};



typedef struct TMGXACT_CHECKPOINT
{
	int						committedCount;
	int						segmentCount;

    /* Array [0..committedCount-1] of TMGXACT_LOG structs begins here */
	TMGXACT_LOG				committedGxactArray[1];
}	TMGXACT_CHECKPOINT;

#define TMGXACT_CHECKPOINT_BYTES(committedCount) \
	(offsetof(TMGXACT_CHECKPOINT, committedGxactArray) + sizeof(TMGXACT_LOG) * (committedCount))


/* From "cdb/cdbexplain.h" */
struct CdbExplain_Agg
{
    double      vmax;           /* maximum value of statistic */
    double      vsum;           /* sum of values */
    int         vcnt;           /* count of values > 0 */
    int         imax;           /* id of 1st observation having maximum value */
};

/* From "cdb/cdbcellbuf.h" */
/*
 * CdbCellBuf
 *   - A container for small fixed-size items.
 */
struct CdbCellBuf
{
    char           *cbeg;               /* first cell in active bunch */
    char           *cfree;              /* end of used cells in active bunch */
    char           *cend;               /* end of cells in active bunch */
    uint16          cellbytes;          /* size of one cell (bytes) */
    int16           offset_nearby;      /* distance in bytes from this struct to
                                         *   nearby cells provided by caller */
    int             nearby_bunch_len;   /* cellbytes * number of nearby cells */
    int             expand_bunch_len;   /* cellbytes * number of cells per
                                         *   expansion bunch */
    long            nfull_total;        /* total number of occupied cells */
    struct CdbCellBuf_Bumper   *head;   /* beginning of first expansion bunch */
    struct CdbCellBuf_Bumper   *tail;   /* beginning of last expansion bunch */
    MemoryContext   context;            /* memory context for expansion */
};


#endif


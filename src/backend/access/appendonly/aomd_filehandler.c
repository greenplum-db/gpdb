/*-------------------------------------------------------------------------
 *
 * aomd_filehandler.c
 *	  Code in this file would have been in aomd.c but is needed in contrib,
 * so we separate it out here.
 *
 * Portions Copyright (c) 2008, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/appendonly/aomd_filehandler.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "access/aomd.h"
#include "access/appendonlytid.h"
#include "access/appendonlywriter.h"

/*
 * Ideally the logic works even for heap tables, but is only used
 * currently for AO and AOCS tables to avoid merge conflicts.
 *
 * There are different rules for the naming of the files, depending on
 * the type of table:
 *
 *   Heap Tables: contiguous extensions, no upper bound
 *   AO Tables: non contiguous extensions [.1 - .127]
 *   CO Tables: non contiguous extensions
 *          [  .1 - .127] for first column;  .0 reserved for utility and alter
 *          [.129 - .255] for second column; .128 reserved for utility and alter
 *          [.257 - .283] for third column;  .256 reserved for utility and alter
 *          etc
 *
 *  Algorithm is coded with the assumption for CO tables that for a given
 *  concurrency level, the relfiles exist OR stop existing for all columns thereafter.
 *  For instance, if .2 exists, then .(2 + 128N) MIGHT exist for N=1.  But if it does
 *  not exist for N=1, then it doesn't exist for N>=2.
 *
 *  We can think of this function as operating on a two-dimensional array:
 *     column index x concurrency level.  The operation is broken up into two
 *     steps:
 *
 *  1) Finds for which concurrency levels the table has files using [.1 - .127].
 *      Concurrency level 0 is always checked as its corresponding segno file
 *      must always exist.  However, the caller is expected to handle the that
 *      file.
 *  2) Iterates over present concurrency levels and uses the above assumption to
 *     stop and proceed to the next concurrency level.
 *
 *  Graphically, showing the step above that can possibly operate on each
 *  segment file:
 *                                 column
 *                              1    2    3    4 ---   MaxHeapAttributeNumber
 *  concurrency 0               x    2)   2)   2)                 2)
 *              1               1)   2)   2)   2)                 2)
 *              2               1)   2)   2)   2)                 2)
 *              3               1)   2)   2)   2)                 2)
 *              |
 *   (MAX_AOREL_CONCURRENCY-1)  1)   2)   2)   2)                 2)
 */
void
ao_foreach_extent_file(ao_extent_callback callback, void *ctx)
{
    int segno;
    int colnum;
    int concurrency[MAX_AOREL_CONCURRENCY];
    int concurrencySize;

    /*
     * We always check concurrency level 0 here as the 0 based extensions such
     * as .128, .256, ... for CO tables are created by ALTER table or utility
     * mode insert. These also need to be copied. Column 0 concurrency level 0
     * file is always present and, as noted above, handled by our caller.
     */
    concurrency[0] = 0;
    concurrencySize = 1;

    /* discover any remaining concurrency levels */
    for (segno = 1; segno < MAX_AOREL_CONCURRENCY; segno++)
    {
        if (!callback(segno, ctx))
            continue;
        concurrency[concurrencySize] = segno;
        concurrencySize++;
    }

    for (int index = 0; index < concurrencySize; index++)
    {
        for (colnum = 1; colnum < MaxHeapAttributeNumber; colnum++)
        {
            segno = colnum * AOTupleId_MultiplierSegmentFileNum + concurrency[index];
            if (!callback(segno, ctx))
                break;
        }
    }
}

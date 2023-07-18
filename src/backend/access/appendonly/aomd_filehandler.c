/*-------------------------------------------------------------------------
 *
 * aomd_filehandler.c
 *	  Code in this file would have been in aomd.c but is needed in contrib,
 * so we separate it out here.
 *
 * Portions Copyright (c) 2008, Greenplum Inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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
 *   CO Tables: non contiguous extensions based on filenums in pg_attribute_encoding
 *          [  .1 - .127] for first filenum;  .0 reserved for utility and alter
 *          [.129 - .255] for second filenum; .128 reserved for utility and alter
 *          [.257 - .283] for third filenum;  .256 reserved for utility and alter
 *          etc upto (128 * MaxFileNumber(3200))
 *
 *  Column rewrites use the filenum from pair (i, i+MaxAttributeNumber)
 *  where i is in range 1 to MaxAttributeNumber
 *  Algorithm is coded with the assumption for CO tables that for a given
 *  concurrency level, the segfiles exist on one of the filenum pairs
 *  OR stop existing for all columns thereafter.
 *  For instance, if .2 exists, then .(2 + 128f) MIGHT exist for filenum f=1.
 *  But if it does not exist for f=1 OR f=1601 then it doesn't exist for f>=2.
 *
 *  We can think of this function as operating on a two-dimensional array:
 *     column index x concurrency level.  The operation is broken up into two
 *     steps:
 *
 *  1) Finds for which concurrency levels the table has files using
 *      [.1 - .127] for filenumber = 1 and same for filenumber = 1601.
 *      Concurrency level 0 is always checked as its corresponding segno file
 *      must always exist.  However, the caller is expected to handle the that
 *      file.
 *  2) Iterates over present concurrency levels and uses the above assumption to
 *     stop and proceed to the next concurrency level.
 *
 *  Graphically, showing the step above that can possibly operate on each
 *  segment file:
 *                                 filenumber
 *                              1    2    3    4 ---        MaxFileNumber
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
	int physicalsegno;
	int physicalsegnopair;
	int filenum;
	int concurrency[MAX_AOREL_CONCURRENCY];
	int concurrencySize;
	bool segnofileexists;
	bool segnopairfileexists;

	/*
	 * We always check concurrency level 0 here as the 0 based extensions such
	 * as .128, .256, ... for CO tables are created by ALTER table or utility
	 * mode insert. These also need to be copied. Column 0 concurrency level 0
	 * file is always present and, as noted above, handled by our caller.
	 */
	concurrency[0] = 0;
	concurrencySize = 1;

	/* 
	 * As we'll see later, we will exhaustively check file extensions that are based
	 * on a combination of all possible segno and filenum except for the base file
	 * (segno=0, filenum=0) as that is not an extension.
	 * But we still need to check (segno=0, filenum=1600) (i.e. .204800) which is the
	 * "pair" extension for the base file. That is not going to be covered by the for
	 * loops below. Check it now.
	 */
	callback(MaxHeapAttributeNumber * AOTupleId_MultiplierSegmentFileNum, ctx);

	/*
	 * Discover any remaining concurrency levels.
	 * This checks all combinations of (segno > 0, filenum = 0) for an AO table, and
	 * additionally (segno > 0, filenum = 1600) for an CO table.
	 */
	for (segno = 1; segno < MAX_AOREL_CONCURRENCY; segno++)
	{
		/* For AOCO tables, each column has two possible file segnos from
		 * filenum pair (i, i+MaxHeapAttributeNumber). Check them both. */
		physicalsegno = segno;
		physicalsegnopair = MaxHeapAttributeNumber * AOTupleId_MultiplierSegmentFileNum + segno;
		segnofileexists = callback(physicalsegno, ctx);
		segnopairfileexists = callback(physicalsegnopair, ctx);
		if (!(segnofileexists || segnopairfileexists))
			continue;
		concurrency[concurrencySize] = segno;
		concurrencySize++;
	}

	/*
	 * Now based on the concurrency levels, discover the rest of file extensions.
	 * This should only be relevant to CO tables.
	 */
	for (int index = 0; index < concurrencySize; index++)
	{
		for (filenum = 1; filenum < MaxHeapAttributeNumber; filenum++)
		{
			physicalsegno = filenum * AOTupleId_MultiplierSegmentFileNum + concurrency[index];
			physicalsegnopair = (filenum + MaxHeapAttributeNumber) * AOTupleId_MultiplierSegmentFileNum + concurrency[index];
			/* Call the callback function on both possible files in filenum pair */
			segnofileexists = callback(physicalsegno, ctx);
			segnopairfileexists = callback(physicalsegnopair, ctx);
			/* If they both don't exist, that means none of the further ones exist */
			if (!(segnofileexists || segnopairfileexists))
				break;
		}
	}
}

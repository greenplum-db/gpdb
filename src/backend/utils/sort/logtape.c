/*-------------------------------------------------------------------------
 *
 * logtape.c
 *	  Management of "logical tapes" within temporary files.
 *
 * This is the greenplum logtape implementation.  The original postgres logtape
 * impl is unnecessarily complex and it prevents many perfomanace optmizations.
 */

/*
 * This module exists to support sorting via multiple merge passes (see
 * tuplesort.c).  Merging is an ideal algorithm for tape devices, but if
 * we implement it on disk by creating a separate file for each "tape",
 * there is an annoying problem: the peak space usage is at least twice
 * the volume of actual data to be sorted.	(This must be so because each
 * datum will appear in both the input and output tapes of the final
 * merge pass.	For seven-tape polyphase merge, which is otherwise a
 * pretty good algorithm, peak usage is more like 4x actual data volume.)
 *
 * We can work around this problem by recognizing that any one tape
 * dataset (with the possible exception of the final output) is written
 * and read exactly once in a perfectly sequential manner.	Therefore,
 * a datum once read will not be required again, and we can recycle its
 * space for use by the new tape dataset(s) being generated.  In this way,
 * the total space usage is essentially just the actual data volume, plus
 * insignificant bookkeeping and start/stop overhead.
 *
 * Few OSes allow arbitrary parts of a file to be released back to the OS,
 * so we have to implement this space-recycling ourselves within a single
 * logical file.  logtape.c exists to perform this bookkeeping and provide
 * the illusion of N independent tape devices to tuplesort.c.  Note that
 * logtape.c itself depends on buffile.c to provide a "logical file" of
 * larger size than the underlying OS may support.
 *
 * For simplicity, we allocate and release space in the underlying file
 * in BLCKSZ-size blocks.  Space allocation boils down to keeping track
 * of which blocks in the underlying file belong to which logical tape,
 * plus any blocks that are free (recycled and not yet reused).
 * The blocks in each logical tape are remembered using a method borrowed
 * from the Unix HFS filesystem: we store data block numbers in an
 * "indirect block".  If an indirect block fills up, we write it out to
 * the underlying file and remember its location in a second-level indirect
 * block.  In the same way second-level blocks are remembered in third-
 * level blocks, and so on if necessary (of course we're talking huge
 * amounts of data here).  The topmost indirect block of a given logical
 * tape is never actually written out to the physical file, but all lower-
 * level indirect blocks will be.
 *
 * The initial write pass is guaranteed to fill the underlying file
 * perfectly sequentially, no matter how data is divided into logical tapes.
 * Once we begin merge passes, the access pattern becomes considerably
 * less predictable --- but the seeking involved should be comparable to
 * what would happen if we kept each logical tape in a separate file,
 * so there's no serious performance penalty paid to obtain the space
 * savings of recycling.  We try to localize the write accesses by always
 * writing to the lowest-numbered free block when we have a choice; it's
 * not clear this helps much, but it can't hurt.  (XXX perhaps a LIFO
 * policy for free blocks would be better?)
 *
 * To support the above policy of writing to the lowest free block,
 * ltsGetFreeBlock sorts the list of free block numbers into decreasing
 * order each time it is asked for a block and the list isn't currently
 * sorted.	This is an efficient way to handle it because we expect cycles
 * of releasing many blocks followed by re-using many blocks, due to
 * tuplesort.c's "preread" behavior.
 *
 * Since all the bookkeeping and buffer memory is allocated with palloc(),
 * and the underlying file(s) are made with OpenTemporaryFile, all resources
 * for a logical tape set are certain to be cleaned up even if processing
 * is aborted by ereport(ERROR).  To avoid confusion, the caller should take
 * care that all calls for a single LogicalTapeSet are made in the same
 * palloc context.
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/utils/sort/logtape.c,v 1.27 2009/01/01 17:23:53 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "executor/execWorkfile.h"
#include "utils/logtape.h"

#include "cdb/cdbvars.h"                /* currentSliceId */


/* A logical tape block, log tape blocks are organized into doulbe linked lists */
#define LOGTAPE_BLK_PAYLOAD_SIZE ((BLCKSZ - sizeof(long)*2 - sizeof(int) ))
typedef struct LogicalTapeBlock
{
	long prev_blk;
	long next_blk;
	int  payload_tail;
	char payload[LOGTAPE_BLK_PAYLOAD_SIZE];
} LogicalTapeBlock ;


/*
 * This data structure represents a single "logical tape" within the set
 * of logical tapes stored in the same file.  We must keep track of the
 * current partially-read-or-written data block as well as the active
 * indirect block level(s).
 */
struct LogicalTape
{
	LogicalTapeBlock currBlk;

	bool		writing;		/* T while in write phase */
	bool		frozen;			/* T if blocks should not be freed when read */

	int64 		firstBlkNum;  /* First block block number */
	LogicalTapePos   currPos;         /* current postion */
};

/*
 * This data structure represents a set of related "logical tapes" sharing
 * space in a single underlying file.  (But that "file" may be multiple files
 * if needed to escape OS limits on file size; buffile.c handles that for us.)
 * The number of tapes is fixed at creation.
 */
struct LogicalTapeSet
{
	ExecWorkFile    *pfile;			/* underlying file for whole tape set */
	long		nFileBlocks;	/* # of blocks used in underlying file */

	/*
	 * We store the numbers of recycled-and-available blocks in freeBlocks[].
	 * When there are no such blocks, we extend the underlying file.
	 *
	 * If forgetFreeSpace is true then any freed blocks are simply forgotten
	 * rather than being remembered in freeBlocks[].  See notes for
	 * LogicalTapeSetForgetFreeSpace().
	 *
	 * If blocksSorted is true then the block numbers in freeBlocks are in
	 * *decreasing* order, so that removing the last entry gives us the lowest
	 * free block.	We re-sort the blocks whenever a block is demanded; this
	 * should be reasonably efficient given the expected usage pattern.
	 */
	bool 		forgetFreeSpace; /* if we need to keep track of free space */
	bool		blocksSorted;	/* is freeBlocks[] currently in order? */
	long	   *freeBlocks;		/* resizable array */
	long		nFreeBlocks;	/* # of currently free blocks */
	long		freeBlocksLen;	/* current allocated length of freeBlocks[] */

	/*
	 * tapes[] is declared size 1 since C wants a fixed size, but actually it
	 * is of length nTapes.
	 */
	int			nTapes;			/* # of logical tapes in set */
	LogicalTape tapes[1];		/* must be last in struct! */
};

static void ltsWriteBlock(LogicalTapeSet *lts, int64 blocknum, void *buffer);
static void ltsReadBlock(LogicalTapeSet *lts, int64 blocknum, void *buffer);
static long ltsGetFreeBlock(LogicalTapeSet *lts);
static void ltsReleaseBlock(LogicalTapeSet *lts, int64 blocknum);
static LogicalTapeSet *LogicalTapeSetCreate_Named(const char *set_prefix, int ntapes, bool del_on_close);

/*
 * Writes state of a LogicalTapeSet to a state file
 */
static void DumpLogicalTapeSetState(ExecWorkFile *statefile, LogicalTapeSet *lts, LogicalTape *lt)
{
	Assert(lts && lt && lt->frozen);

	bool res = ExecWorkFile_Write(statefile, &(lts->nFileBlocks), sizeof(lts->nFileBlocks));
	Assert(res);

	res = ExecWorkFile_Write(statefile, &(lt->firstBlkNum), sizeof(lt->firstBlkNum));
	Assert(res);
}

/*
 * Loads the state of a LogicaTapeSet from a BufFile
 *
 *   statefile is an open ExecWorkFile containing the state of the LogicalTapeSet
 *   tapefile is an open ExecWorkfile containing the tapeset
 */
LogicalTapeSet *
LoadLogicalTapeSetState(ExecWorkFile *statefile, ExecWorkFile *tapefile)
{
	Assert(NULL != statefile);
	Assert(NULL != tapefile);

	LogicalTapeSet *lts;
	LogicalTape *lt;
	size_t readSize;

	lts = (LogicalTapeSet *) palloc(sizeof(LogicalTapeSet));
	lts->pfile = tapefile;
	lts->nTapes = 1;
	lt = &lts->tapes[0];

	readSize = ExecWorkFile_Read(statefile, &(lts->nFileBlocks), sizeof(lts->nFileBlocks));
	if(readSize != sizeof(lts->nFileBlocks))
		elog(ERROR, "Load logicaltapeset failed to read nFileBlocks");

	/* For loaded tape, we will read only and do not care about free space */
	lts->forgetFreeSpace = true;
	lts->blocksSorted = true;
	lts->freeBlocks = NULL;
	lts->nFreeBlocks = 0;
	lts->freeBlocksLen = 0;

	lt->writing = false;
	lt->frozen = true;

	readSize = ExecWorkFile_Read(statefile, &(lt->firstBlkNum), sizeof(lt->firstBlkNum));
	if(readSize != sizeof(lt->firstBlkNum))
		elog(ERROR, "Load logicaltapeset failed to read tape firstBlkNum");

	if(lt->firstBlkNum != -1)
		ltsReadBlock(lts, lt->firstBlkNum, &lt->currBlk);

	lt->currPos.blkNum = lt->firstBlkNum;
	lt->currPos.offset = 0;

	return lts;
}
	
/*
 * Write a block-sized buffer to the specified block of the underlying file.
 *
 * NB: should not attempt to write beyond current end of file (ie, create
 * "holes" in file), since BufFile doesn't allow that.  The first write pass
 * must write blocks sequentially.
 *
 * No need for an error return convention; we ereport() on any error.
 */
static void
ltsWriteBlock(LogicalTapeSet *lts, int64 blocknum, void *buffer)
{
	Assert(lts != NULL);
	if (ExecWorkFile_Seek(lts->pfile, blocknum * BLCKSZ, SEEK_SET) != 0 ||
			!ExecWorkFile_Write(lts->pfile, buffer, BLCKSZ))
	{
		ereport(ERROR,
		/* XXX is it okay to assume errno is correct? */
				(errcode_for_file_access(),
				 errmsg("could not write block " INT64_FORMAT  " of temporary file: %m",
						blocknum),
				 errhint("Perhaps out of disk space?")));
	}
}

/*
 * Read a block-sized buffer from the specified block of the underlying file.
 *
 * No need for an error return convention; we ereport() on any error.	This
 * module should never attempt to read a block it doesn't know is there.
 */
static void
ltsReadBlock(LogicalTapeSet *lts, int64 blocknum, void *buffer)
{
	Assert(lts != NULL);
	if (ExecWorkFile_Seek(lts->pfile, blocknum * BLCKSZ, SEEK_SET) != 0 ||
			ExecWorkFile_Read(lts->pfile, buffer, BLCKSZ) != BLCKSZ)
	{
		ereport(ERROR,
		/* XXX is it okay to assume errno is correct? */
				(errcode_for_file_access(),
				 errmsg("could not read block " INT64_FORMAT  " of temporary file: %m",
						blocknum)));
	}
}

/*
 * qsort comparator for sorting freeBlocks[] into decreasing order.
 */
static int
freeBlocks_cmp(const void *a, const void *b)
{
	long		ablk = *((const long *) a);
	long		bblk = *((const long *) b);

	/* can't just subtract because long might be wider than int */
	if (ablk < bblk)
		return 1;
	if (ablk > bblk)
		return -1;
	return 0;
}

/*
 * Select a currently unused block for writing to.
 *
 * NB: should only be called when writer is ready to write immediately,
 * to ensure that first write pass is sequential.
 */
static long
ltsGetFreeBlock(LogicalTapeSet *lts)
{
	/*
	 * If there are multiple free blocks, we select the one appearing last in
	 * freeBlocks[] (after sorting the array if needed).  If there are none,
	 * assign the next block at the end of the file.
	 */
	if (lts->nFreeBlocks > 0)
	{
		if (!lts->blocksSorted)
		{
			qsort((void *) lts->freeBlocks, lts->nFreeBlocks,
				  sizeof(long), freeBlocks_cmp);
			lts->blocksSorted = true;
		}
		return lts->freeBlocks[--lts->nFreeBlocks];
	}
	else
		return lts->nFileBlocks++;
}

/*
 * Return a block# to the freelist.
 */
static void
ltsReleaseBlock(LogicalTapeSet *lts, int64 blocknum)
{
	long		ndx;

	/*
	 * Do nothing if we're no longer interested in remembering free space.
	 */
	if (lts->forgetFreeSpace)
		return;

	/*
	 * Enlarge freeBlocks array if full.
	 */
	if (lts->nFreeBlocks >= lts->freeBlocksLen)
	{
		lts->freeBlocksLen *= 2;
		lts->freeBlocks = (long *) repalloc(lts->freeBlocks,
										  lts->freeBlocksLen * sizeof(long));
	}

	/*
	 * Add blocknum to array, and mark the array unsorted if it's no longer in
	 * decreasing order.
	 */
	ndx = lts->nFreeBlocks++;
	lts->freeBlocks[ndx] = blocknum;
	if (ndx > 0 && lts->freeBlocks[ndx - 1] < blocknum)
		lts->blocksSorted = false;
}

/* 
 * Create a logical tape
 */
LogicalTape *
LogicalTapeCreate(LogicalTapeSet *lts, LogicalTape *lt)
{
	Assert(sizeof(LogicalTapeBlock) == BLCKSZ);

	if(lt == NULL)
		lt = (LogicalTape *) palloc(sizeof(LogicalTape));

	lt->writing = true;
	lt->frozen = false;
	lt->firstBlkNum = -1L;
	lt->currPos.blkNum = -1L;
	lt->currPos.offset = 0;
	return lt;
}

/*
 * Create a set of logical tapes in a temporary underlying file.
 *
 * Each tape is initialized in write state.
 */
static LogicalTapeSet *
LogicalTapeSetCreate_Internal(int ntapes)
{
	LogicalTapeSet *lts;
	int			i;

	/*
	 * Create top-level struct including per-tape LogicalTape structs. First
	 * LogicalTape struct is already counted in sizeof(LogicalTapeSet).
	 */
	Assert(ntapes > 0);
	lts = (LogicalTapeSet *) palloc(sizeof(LogicalTapeSet) +
									(ntapes - 1) *sizeof(LogicalTape));
	lts->pfile = NULL; 
	lts->nFileBlocks = 0L;
	lts->forgetFreeSpace = false;
	lts->blocksSorted = true;	/* a zero-length array is sorted ... */
	lts->freeBlocksLen = 32;	/* reasonable initial guess */
	lts->freeBlocks = (long *) palloc(lts->freeBlocksLen * sizeof(long));
	lts->nFreeBlocks = 0;
	lts->nTapes = ntapes;

	/*
	 * Initialize per-tape structs.  Note we allocate the I/O buffer and
	 * first-level indirect block for a tape only when it is first actually
	 * written to.	This avoids wasting memory space when tuplesort.c
	 * overestimates the number of tapes needed.
	 */
	for (i = 0; i < ntapes; i++)
		LogicalTapeCreate(lts, &lts->tapes[i]); 

	return lts;
}

/*
 * Creates a LogicalTapeSet with a generated file name.
 */
LogicalTapeSet *LogicalTapeSetCreate(int ntapes, bool del_on_close)
{
	char tmpprefix[MAXPGPATH];
	int len = snprintf(tmpprefix, MAXPGPATH, "%s/slice%d_sort",
			PG_TEMP_FILES_DIR,
			currentSliceId);
	insist_log(len <= MAXPGPATH - 1, "could not generate temporary file name");
	StringInfo uniquename = ExecWorkFile_AddUniqueSuffix(tmpprefix);

	LogicalTapeSet *lts = LogicalTapeSetCreate_Named(uniquename->data, ntapes, del_on_close);

	pfree(uniquename->data);
	pfree(uniquename);

	return lts;
}

/*
 * Creates a LogicalTapeSet with a given name.
 *
 * Note: Requires the pgsql_tmp/ directory to be part of the prefix
 */
static LogicalTapeSet *
LogicalTapeSetCreate_Named(const char *set_prefix, int ntapes, bool del_on_close)
{
	LogicalTapeSet *lts = LogicalTapeSetCreate_Internal(ntapes);
	lts->pfile = ExecWorkFile_Create(set_prefix, BUFFILE, del_on_close, 0 /* compressType */);
	return lts;
}

/*
 * Creates a LogicalTapeSet with a given underlying file
 */
LogicalTapeSet *LogicalTapeSetCreate_File(ExecWorkFile *ewfile, int ntapes)
{
	LogicalTapeSet *lts = LogicalTapeSetCreate_Internal(ntapes);
	lts->pfile = ewfile;
	return lts;
}

/*
 * Close a logical tape set and release all resources.
 */
void
LogicalTapeSetClose(LogicalTapeSet *lts, workfile_set *workset)
{
	Assert(lts != NULL);
	workfile_mgr_close_file(workset, lts->pfile);
	if(lts->freeBlocks)
		pfree(lts->freeBlocks);
	pfree(lts);
}

/*
 * Mark a logical tape set as not needing management of free space anymore.
 *
 * This should be called if the caller does not intend to write any more data
 * into the tape set, but is reading from un-frozen tapes.	Since no more
 * writes are planned, remembering free blocks is no longer useful.  Setting
 * this flag lets us avoid wasting time and space in ltsReleaseBlock(), which
 * is not designed to handle large numbers of free blocks.
 */
void
LogicalTapeSetForgetFreeSpace(LogicalTapeSet *lts)
{
	lts->forgetFreeSpace = true;
}

/*
 * Write to a logical tape.
 *
 * There are no error returns; we ereport() on failure.
 */
void
LogicalTapeWrite(LogicalTapeSet *lts, LogicalTape *lt,
				 void *ptr, size_t size)
{
	long        tmpBlkNum;
	size_t		nthistime;

	Assert(lt->writing);

	if (lt->firstBlkNum == -1)
	{
		lt->firstBlkNum = ltsGetFreeBlock(lts);
		lt->currBlk.prev_blk = -1L;
		lt->currBlk.next_blk = -1L;
		lt->currBlk.payload_tail = 0;

		lt->currPos.blkNum = lt->firstBlkNum;
		lt->currPos.offset = 0;
	}

	while (size > 0)
	{
		Assert(lt->currPos.offset == lt->currBlk.payload_tail);
		Assert(lt->currPos.offset <= LOGTAPE_BLK_PAYLOAD_SIZE);

		if (lt->currPos.offset == LOGTAPE_BLK_PAYLOAD_SIZE)
		{
			Assert(lt->currBlk.payload_tail == LOGTAPE_BLK_PAYLOAD_SIZE);
			tmpBlkNum = ltsGetFreeBlock(lts);
			lt->currBlk.next_blk = tmpBlkNum;
			ltsWriteBlock(lts, lt->currPos.blkNum, &(lt->currBlk));
			lt->currBlk.prev_blk = lt->currPos.blkNum;
			lt->currBlk.next_blk = -1L;
			lt->currBlk.payload_tail = 0;
			lt->currPos.blkNum = tmpBlkNum;
			lt->currPos.offset = 0;
		}

		nthistime = size > (LOGTAPE_BLK_PAYLOAD_SIZE - lt->currPos.offset) ?
			(LOGTAPE_BLK_PAYLOAD_SIZE - lt->currPos.offset) : size;

		memcpy(lt->currBlk.payload + lt->currBlk.payload_tail, ptr, nthistime);
		ptr = (void *) ((char *) ptr + nthistime);
		lt->currBlk.payload_tail += nthistime;
		lt->currPos.offset += nthistime;
		size -= nthistime;
	}
}

/*
 * Rewind logical tape and switch from writing to reading or vice versa.
 *
 * Unless the tape has been "frozen" in read state, forWrite must be the
 * opposite of the previous tape state.
 */
void
LogicalTapeRewind(LogicalTapeSet *lts, LogicalTape *lt, bool forWrite)
{
	AssertEquivalent(lt->firstBlkNum==-1, lt->currPos.blkNum == -1);

	if (!forWrite)
	{
		if (lt->writing)
		{
			if(lt->firstBlkNum != -1)
			{
				Assert(lt->currBlk.next_blk == -1L);
				ltsWriteBlock(lts, lt->currPos.blkNum, &lt->currBlk);

				if(lt->currPos.blkNum != lt->firstBlkNum)
					ltsReadBlock(lts, lt->firstBlkNum, &lt->currBlk);
			}
			
			lt->currPos.blkNum = lt->firstBlkNum;
			lt->currPos.offset = 0;
			lt->writing = false;
		}
		else
		{
			/*
			 * This is only OK if tape is frozen; we rewind for (another) read
			 * pass.
			 */
			Assert(lt->frozen);
			if(lt->currPos.blkNum != lt->firstBlkNum)
				ltsReadBlock(lts, lt->firstBlkNum, &lt->currBlk);

			lt->currPos.blkNum = lt->firstBlkNum;
			lt->currPos.offset = 0;
		}
	}
	else
	{
		lt->firstBlkNum = -1L;
		lt->currBlk.prev_blk = -1L;
		lt->currBlk.next_blk = -1L;
		lt->currBlk.payload_tail = 0;
		lt->currPos.blkNum = -1L;
		lt->currPos.offset = 0;
		lt->writing = true;
	}
}

/*
 * Read from a logical tape.
 *
 * Early EOF is indicated by return value less than #bytes requested.
 */
size_t
LogicalTapeRead(LogicalTapeSet *lts, LogicalTape *lt,
				void *ptr, size_t size)
{
	size_t		nread = 0;
	size_t		nthistime;

	Assert(!lt->writing);

	if(lt->currPos.blkNum == -1)
		return nread;

	while (size > 0)
	{
		Assert(lt->currPos.offset <= lt->currBlk.payload_tail);
		if(lt->currPos.offset == lt->currBlk.payload_tail)
		{
			if(lt->currBlk.next_blk == -1)
			{
				if(!lt->frozen)
				{
					ltsReleaseBlock(lts, lt->currPos.blkNum);
					lt->firstBlkNum = -1L;
					lt->currPos.blkNum = -1L;
					lt->currPos.offset = 0;
				}
				return nread;
			}
			
			lt->currPos.blkNum = lt->currBlk.next_blk;
			lt->currPos.offset = 0;
			ltsReadBlock(lts, lt->currBlk.next_blk, &lt->currBlk);

			if(!lt->frozen)
			{
				ltsReleaseBlock(lts, lt->currBlk.prev_blk);
				lt->firstBlkNum = lt->currPos.blkNum;
			}
		}

		if(lt->currPos.offset < lt->currBlk.payload_tail)
		{
			nthistime = size > (lt->currBlk.payload_tail - lt->currPos.offset) ?
				lt->currBlk.payload_tail - lt->currPos.offset :
				size;

			memcpy(ptr, lt->currBlk.payload + lt->currPos.offset, nthistime);
			size -= nthistime;
			ptr = (void *) ((char *) ptr + nthistime);
			lt->currPos.offset += nthistime;
			nread += nthistime;
		}
	}

	return nread;
}

/*
 * "Freeze" the contents of a tape so that it can be read multiple times
 * and/or read backwards.  Once a tape is frozen, its contents will not
 * be released until the LogicalTapeSet is destroyed.  This is expected
 * to be used only for the final output pass of a merge.
 *
 * This *must* be called just at the end of a write pass, before the
 * tape is rewound (after rewind is too late!).  It performs a rewind
 * and switch to read mode "for free".	An immediately following rewind-
 * for-read call is OK but not necessary.
 */
void
LogicalTapeFreeze(LogicalTapeSet *lts, LogicalTape *lt) 
{
	Assert(lt->writing);

	LogicalTapeRewind(lts, lt, false); 
	lt->writing = false;
	lt->frozen = true;
}

/* Flush the destination tape so the logtapeset can be used by shareinput.
 * We assume the tape has been frozen before this call 
 */
void
LogicalTapeFlush(LogicalTapeSet *lts, LogicalTape *lt, ExecWorkFile *pstatefile)
{
	Assert(lts && lts->pfile);
	Assert(lt->frozen);

	ExecWorkFile_Flush(lts->pfile);
	DumpLogicalTapeSetState(pstatefile, lts, lt);
}

/*
 * Backspace the tape a given number of bytes.	(We also support a more
 * general seek interface, see below.)
 *
 * *Only* a frozen-for-read tape can be backed up; we don't support
 * random access during write, and an unfrozen read tape may have
 * already discarded the desired data!
 *
 * Return value is TRUE if seek successful, FALSE if there isn't that much
 * data before the current point (in which case there's no state change).
 */
bool
LogicalTapeBackspace(LogicalTapeSet *lts, LogicalTape *lt, size_t size)
{
	Assert(lt && lt->frozen);
	while(size > 0)
	{
		if(lt->currPos.blkNum == -1)
			return false;

		if(size <= lt->currPos.offset)
		{
			lt->currPos.offset -= size;
			return true;
		}
	
		size -= lt->currPos.offset;
		
		if(lt->currBlk.prev_blk == -1L)
			return false;

		lt->currPos.blkNum = lt->currBlk.prev_blk;
		ltsReadBlock(lts, lt->currPos.blkNum, &lt->currBlk);
		lt->currPos.offset = lt->currBlk.payload_tail;
	}

	return true;
}

/* Get a logical tape given tape number */
LogicalTape *LogicalTapeSetGetTape(LogicalTapeSet *lts, int tapenum)
{
	Assert(tapenum >= 0 && tapenum < lts->nTapes);
	return &lts->tapes[tapenum];
}

/*
 * Seek to an arbitrary position in a logical tape.
 *
 * *Only* a frozen-for-read tape can be seeked. 
 * We assume seek tape will seek on a pos that returned by tell, that is, the pos is a 
 * valid postion on this tape.  The return value is ALWAYS true.
 */
bool
LogicalTapeSeek(LogicalTapeSet *lts, LogicalTape *lt, LogicalTapePos *pos)
{
	Assert(lt->frozen);
	Assert(lt->firstBlkNum != -1L);

	if(pos->blkNum != lt->currPos.blkNum)
		ltsReadBlock(lts, pos->blkNum, &lt->currBlk);

	lt->currPos.blkNum = pos->blkNum;
	lt->currPos.offset = pos->offset;

	return true;
}

/*
 * Obtain current position in a form suitable for a later LogicalTapeSeek.
 */
void
LogicalTapeTell(LogicalTapeSet *lts, LogicalTape *lt, LogicalTapePos *pos)
{
	Assert(lt->frozen);
	pos->blkNum = lt->currPos.blkNum;
	pos->offset = lt->currPos.offset;
}

/*
 * Obtain current position from an unfrozen tape.
 */
void
LogicalTapeUnfrozenTell(LogicalTapeSet *lts, LogicalTape *lt, LogicalTapePos *pos)
{
	pos->blkNum = lt->currPos.blkNum;
	pos->offset = lt->currPos.offset;
}

/*
 * Obtain total disk space currently used by a LogicalTapeSet, in blocks.
 */
long
LogicalTapeSetBlocks(LogicalTapeSet *lts)
{
	return lts->nFileBlocks;
}

LogicalTape *LogicalTapeSetDuplicateTape(LogicalTapeSet *lts, LogicalTape *lt)
{
	LogicalTape *dup = (LogicalTape *) palloc(sizeof(LogicalTape));

	Assert(lt->frozen);
	memcpy(dup, lt, sizeof(LogicalTape));

	return dup;
}

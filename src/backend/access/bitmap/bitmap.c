/*-------------------------------------------------------------------------
 *
 * bitmap.c
 *	  Implementation of the Hybrid Run-Length (HRL) on-disk bitmap index.
 *
 * Portions Copyright (c) 2007-2010 Greenplum Inc
 * Portions Copyright (c) 2010-2012 EMC Corporation
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 2006-2008, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/bitmap/bitmap.c
 *
 * NOTES
 *	This file contains only the public interface routines.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "access/bitmap.h"
#include "access/xact.h"
#include "catalog/index.h"
#include "miscadmin.h"
#include "nodes/tidbitmap.h"
#include "storage/lmgr.h"
#include "storage/smgr.h"
#include "parser/parse_oper.h"
#include "utils/memutils.h"

static void bmbuildCallback(Relation index,	ItemPointer tupleId, Datum *attdata,
							bool *nulls, bool tupleIsAlive,	void *state);
static bool words_get_match(BMBatchWords *words, BMIterateResult *result,
                            BlockNumber blockno, PagetableEntry *entry,
							bool newentry);
static IndexScanDesc copy_scan_desc(IndexScanDesc scan);
static void stream_free(StreamNode *self);
static void indexstream_free(StreamNode *self);
static bool pull_stream(StreamNode *self, PagetableEntry *e);
static void cleanup_pos(BMScanPosition pos);

/* type to hide BM specific stream state */
typedef struct BMStreamOpaque
{
	IndexScanDesc scan;
	PagetableEntry *entry;
	/* Indicate that this stream contains no more bitmap words. */
	bool is_done;
} BMStreamOpaque;

/*
 * bmbuild() -- Build a new bitmap index.
 */
Datum
bmbuild(PG_FUNCTION_ARGS)
{
	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_DECLARE;

	Relation    heap = (Relation) PG_GETARG_POINTER(0);
	Relation    index = (Relation) PG_GETARG_POINTER(1);
	IndexInfo  *indexInfo = (IndexInfo *) PG_GETARG_POINTER(2);
	double      reltuples;
	BMBuildState bmstate;
	IndexBuildResult *result;
	TupleDesc	tupDesc;

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_ENTER;

	if (indexInfo->ii_Concurrent)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("CONCURRENTLY is not supported when creating bitmap indexes")));

	/* We expect this to be called exactly once. */
	if (RelationGetNumberOfBlocks(index) != 0)
		ereport (ERROR,
				(errcode(ERRCODE_INDEX_CORRUPTED),
				errmsg("index \"%s\" already contains data",
				RelationGetRelationName(index)),
				errSendAlert(true)));

	tupDesc = RelationGetDescr(index);

	/* initialize the bitmap index. */
	_bitmap_init(index, !index->rd_istemp);

	/* initialize the build state. */
	_bitmap_init_buildstate(index, &bmstate);

	/* do the heap scan */
	reltuples = IndexBuildScan(heap, index, indexInfo, false,
							  bmbuildCallback, (void *)&bmstate);
	/* clean up the build state */
	_bitmap_cleanup_buildstate(index, &bmstate);
	
	/* return statistics */
	result = (IndexBuildResult *) palloc0(sizeof(IndexBuildResult));

	result->heap_tuples = reltuples;
	result->index_tuples = bmstate.ituples;

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_EXIT;

	PG_RETURN_POINTER(result);
}


/*
 * bminsert() -- insert an index tuple into a bitmap index.
 */
Datum
bminsert(PG_FUNCTION_ARGS)
{
	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_DECLARE;

	Relation	rel = (Relation) PG_GETARG_POINTER(0);
	Datum		*datum = (Datum *) PG_GETARG_POINTER(1);
	bool		*nulls = (bool *) PG_GETARG_POINTER(2);
	ItemPointer	ht_ctid = (ItemPointer) PG_GETARG_POINTER(3);

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_ENTER;

	_bitmap_doinsert(rel, *ht_ctid, datum, nulls);

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_EXIT;

	PG_RETURN_BOOL(true);
}

/*
 * bmgettuple() -- return the next tuple in a scan.
 */
Datum
bmgettuple(PG_FUNCTION_ARGS)
{
	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_DECLARE;

	IndexScanDesc scan = (IndexScanDesc) PG_GETARG_POINTER(0);
	ScanDirection dir = (ScanDirection) PG_GETARG_INT32(1);
	BMScanOpaque  so = (BMScanOpaque)scan->opaque;

	bool res;

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_ENTER;

	/* This implementation of a bitmap index is never lossy */
	scan->xs_recheck = false;

	/* 
	 * If we have already begun our scan, continue in the same direction.
	 * Otherwise, start up the scan.
	 */
	if (so->bm_currPos && so->cur_pos_valid)
		res = _bitmap_next(scan, dir);
	else
		res = _bitmap_first(scan, dir);

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_EXIT;

	PG_RETURN_BOOL(res);
}

/*
 * bmgetbitmap() -- return a stream bitmap.
 */
Datum
bmgetbitmap(PG_FUNCTION_ARGS)
{
	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_DECLARE;

	/* We ignore the second argument as we're returning a hash bitmap */
	IndexScanDesc scan = (IndexScanDesc) PG_GETARG_POINTER(0);
	Node		 *bm = (Node *)PG_GETARG_POINTER(1);
	IndexStream	 *is;
	BMScanPosition	scanPos;
	bool res;

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_ENTER;

	res = _bitmap_firstbatchwords(scan, ForwardScanDirection);

	scanPos = ((BMScanOpaque)scan->opaque)->bm_currPos;
	scanPos->bm_result.nextTid = 1;

	if (res)
	{
		BMScanPosition  sp;
		IndexScanDesc copy = copy_scan_desc(scan);
		BMStreamOpaque *so;
		int vec;

		/* perhaps this should be in a special context? */
		is = (IndexStream *)palloc0(sizeof(IndexStream));
		is->type = BMS_INDEX;
		is->pull = pull_stream;
		is->nextblock = 0;
		is->free = indexstream_free;
		is->set_instrument = NULL;
		is->upd_instrument = NULL;

		/* create a memory context for the stream */

		so = palloc(sizeof(BMStreamOpaque));
		sp = ((BMScanOpaque)copy->opaque)->bm_currPos;
		so->scan = copy;
		so->entry = NULL;
		so->is_done = false;
		is->opaque = (void *)so;

		if(!bm)
		{
			/* 
			 * We must create the StreamBitmap outside of our temporary
			 * memory context. The reason is, because we glue all the 
			 * related streams together, bitmap_stream_free() will
			 * descend the stream tree and free up all the nodes by
			 * killing their memory context. If we lose the StreamBitmap
			 * memory, we'll be reading invalid memory.
			 */
			StreamBitmap *sb = makeNode(StreamBitmap);
			sb->streamNode = is;
			bm = (Node *)sb;
		}
		else if(IsA(bm, StreamBitmap))
		{
			stream_add_node((StreamBitmap *)bm, is, BMS_OR);
		}
		else
		{
			elog(ERROR, "non stream bitmap"); 
		}

		/*
		 * Since we have made a copy for this scan, we reset the lov buffers
		 * in the original scan to make sure that these buffers will not
		 * be released.
		 */
		for (vec = 0; vec < scanPos->nvec; vec++)
		{
			BMVector bmvec = &(scanPos->posvecs[vec]);
			bmvec->bm_lovBuffer = InvalidBuffer;
		}
	}

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_EXIT;

	PG_RETURN_POINTER(bm);
}

/*
 * bmbeginscan() -- start a scan on the bitmap index.
 */
Datum
bmbeginscan(PG_FUNCTION_ARGS)
{
	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_DECLARE;

	Relation	rel = (Relation) PG_GETARG_POINTER(0);
	int			nkeys = PG_GETARG_INT32(1);
	ScanKey		scankey = (ScanKey) PG_GETARG_POINTER(2);
	IndexScanDesc scan;

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_ENTER;

	/* get the scan */
	scan = RelationGetIndexScan(rel, nkeys, scankey);

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_EXIT;

	PG_RETURN_POINTER(scan);
}

/*
 * bmrescan() -- restart a scan on the bitmap index.
 */
Datum
bmrescan(PG_FUNCTION_ARGS)
{
	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_DECLARE;

	IndexScanDesc	scan = (IndexScanDesc) PG_GETARG_POINTER(0);
	ScanKey			scankey = (ScanKey) PG_GETARG_POINTER(1);
	BMScanOpaque	so = (BMScanOpaque) scan->opaque;

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_ENTER;

	/* so will be NULL if we were called via index_rescan() */
	if (so == NULL)
	{
		so = (BMScanOpaque) palloc(sizeof(BMScanOpaqueData));
		so->bm_currPos = NULL;
		so->bm_markPos = NULL;
		so->cur_pos_valid = false;
		so->mark_pos_valid = false;
		scan->opaque = so;
	}

	if (so->bm_currPos != NULL)
	{
		cleanup_pos(so->bm_currPos);
		MemSet(so->bm_currPos, 0, sizeof(BMScanPositionData));
		so->cur_pos_valid = false;
	}

	if (so->bm_markPos != NULL)
	{
		cleanup_pos(so->bm_markPos);
		MemSet(so->bm_markPos, 0, sizeof(BMScanPositionData));
		so->cur_pos_valid = false;
	}
	/* reset the scan key */
	if (scankey && scan->numberOfKeys > 0)
		memmove(scan->keyData, scankey,
				scan->numberOfKeys * sizeof(ScanKeyData));

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_EXIT;

	PG_RETURN_VOID();
}

/*
 * bmendscan() -- close a scan.
 */
Datum
bmendscan(PG_FUNCTION_ARGS)
{
	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_DECLARE;

	IndexScanDesc	scan = (IndexScanDesc) PG_GETARG_POINTER(0);
	BMScanOpaque	so = (BMScanOpaque) scan->opaque;

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_ENTER;

	/* free the space */
	if (so->bm_currPos != NULL)
	{
		/*
		 * release the buffers that have been stored for each related 
		 * bitmap vector.
		 */
		if (so->bm_currPos->nvec > 1)
			 _bitmap_cleanup_batchwords(so->bm_currPos->bm_batchWords);
		_bitmap_cleanup_scanpos(so->bm_currPos->posvecs,
								so->bm_currPos->nvec);
		pfree(so->bm_currPos);
		so->bm_currPos = NULL;
	}

	if (so->bm_markPos != NULL)
	{
		if (so->bm_markPos->nvec > 1)
			 _bitmap_cleanup_batchwords(so->bm_markPos->bm_batchWords);
		_bitmap_cleanup_scanpos(so->bm_markPos->posvecs,
								so->bm_markPos->nvec);
		so->bm_markPos = NULL;
	}

	pfree(so);
	scan->opaque = NULL;

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_EXIT;

	PG_RETURN_VOID();
}

/*
 * bmmarkpos() -- save the current scan position.
 */
Datum
bmmarkpos(PG_FUNCTION_ARGS)
{
	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_DECLARE;

	IndexScanDesc	scan = (IndexScanDesc) PG_GETARG_POINTER(0);
	BMScanOpaque	so = (BMScanOpaque) scan->opaque;
	BMVector	bmScanPos;
	uint32 vectorNo;

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_ENTER;

	/* free the space */
	if (so->mark_pos_valid)
	{
		/*
		 * release the buffers that have been stored for each
		 * related bitmap.
		 */
		bmScanPos = so->bm_markPos->posvecs;

		for (vectorNo=0; vectorNo < so->bm_markPos->nvec; vectorNo++)
		{
			if (BufferIsValid((bmScanPos[vectorNo]).bm_lovBuffer))
			{
				ReleaseBuffer((bmScanPos[vectorNo]).bm_lovBuffer);
				(bmScanPos[vectorNo]).bm_lovBuffer = InvalidBuffer;
			}
		}
		so->mark_pos_valid = false;
	}

	if (so->cur_pos_valid)
	{
		uint32	size = sizeof(BMScanPositionData);


		/* set the mark position */
		if (so->bm_markPos == NULL)
		{
			so->bm_markPos = (BMScanPosition) palloc(size);
		}

		bmScanPos = so->bm_currPos->posvecs;

		for (vectorNo = 0; vectorNo < so->bm_currPos->nvec; vectorNo++)
		{
			if (BufferIsValid((bmScanPos[vectorNo]).bm_lovBuffer))
				IncrBufferRefCount((bmScanPos[vectorNo]).bm_lovBuffer);
		}

		memcpy(so->bm_markPos->posvecs, bmScanPos,
			   so->bm_currPos->nvec *
			   sizeof(BMVectorData));
		memcpy(so->bm_markPos, so->bm_currPos, size);

		so->mark_pos_valid = true;
	}

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_EXIT;

	PG_RETURN_VOID();
}

/*
 * bmrestrpos() -- restore a scan to the last saved position.
 */
Datum
bmrestrpos(PG_FUNCTION_ARGS)
{
	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_DECLARE;

	IndexScanDesc	scan = (IndexScanDesc) PG_GETARG_POINTER(0);
	BMScanOpaque	so = (BMScanOpaque) scan->opaque;

	BMVector	bmScanPos;
	uint32 vectorNo;

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_ENTER;

	/* free space */
	if (so->cur_pos_valid)
	{
		/* release the buffers that have been stored for each related bitmap.*/
		bmScanPos = so->bm_currPos->posvecs;

		for (vectorNo=0; vectorNo<so->bm_markPos->nvec;
			 vectorNo++)
		{
			if (BufferIsValid((bmScanPos[vectorNo]).bm_lovBuffer))
			{
				ReleaseBuffer((bmScanPos[vectorNo]).bm_lovBuffer);
				(bmScanPos[vectorNo]).bm_lovBuffer = InvalidBuffer;
			}
		}
		so->cur_pos_valid = false;
	}

	if (so->mark_pos_valid)
	{
		uint32	size = sizeof(BMScanPositionData);

		/* set the current position */
		if (so->bm_currPos == NULL)
		{
			so->bm_currPos = (BMScanPosition) palloc0(size);
		}

		bmScanPos = so->bm_markPos->posvecs;

		for (vectorNo=0; vectorNo<so->bm_currPos->nvec;
			 vectorNo++)
		{
			if (BufferIsValid((bmScanPos[vectorNo]).bm_lovBuffer))
				IncrBufferRefCount((bmScanPos[vectorNo]).bm_lovBuffer);
		}		

		memcpy(so->bm_currPos->posvecs, bmScanPos,
			   so->bm_markPos->nvec *
			   sizeof(BMVectorData));
		memcpy(so->bm_currPos, so->bm_markPos, size);
		so->cur_pos_valid = true;
	}

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_EXIT;

	PG_RETURN_VOID();
}

/*
 * bmbulkdelete() -- bulk delete index entries
 *
 * Re-index is performed before retrieving the number of tuples
 * indexed in this index.
 */
Datum
bmbulkdelete(PG_FUNCTION_ARGS)
{
	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_DECLARE;

	IndexVacuumInfo *info = (IndexVacuumInfo *) PG_GETARG_POINTER(0);
	Relation	rel = info->index;
	IndexBulkDeleteResult* volatile result =
		(IndexBulkDeleteResult *) PG_GETARG_POINTER(1);

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_ENTER;

	/* allocate stats if first time through, else re-use existing struct */
	if (result == NULL)
		result = (IndexBulkDeleteResult *)
			palloc0(sizeof(IndexBulkDeleteResult));	

	reindex_index(RelationGetRelid(rel));

	CommandCounterIncrement();

	result = (IndexBulkDeleteResult *) palloc0(sizeof(IndexBulkDeleteResult));
	result->num_pages = RelationGetNumberOfBlocks(rel);
	/* Since we re-build the index, set this to number of heap tuples. */
	result->num_index_tuples = info->num_heap_tuples;
	result->tuples_removed = 0;

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_EXIT;

	PG_RETURN_POINTER(result);
}

/*
 * bmvacuumcleanup() -- post-vacuum cleanup.
 *
 * We do nothing useful here.
 */
Datum
bmvacuumcleanup(PG_FUNCTION_ARGS)
{
	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_DECLARE;

	IndexVacuumInfo *info = (IndexVacuumInfo *) PG_GETARG_POINTER(0);
	Relation	rel = info->index;
	IndexBulkDeleteResult *stats = 
			(IndexBulkDeleteResult *) PG_GETARG_POINTER(1);

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_ENTER;

	if(stats == NULL)
		stats = (IndexBulkDeleteResult *)palloc0(sizeof(IndexBulkDeleteResult));

	/* update statistics */
	stats->num_pages = RelationGetNumberOfBlocks(rel);
	stats->pages_deleted = 0;
	stats->pages_free = 0;
	/* XXX: dodgy hack to shutup index_scan() and vacuum_index() */
	stats->num_index_tuples = info->num_heap_tuples;

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_EXIT;

	PG_RETURN_POINTER(stats);
}

/*
 * Per-tuple callback from IndexBuildHeapScan
 */
static void
bmbuildCallback(Relation index, ItemPointer tupleId, Datum *attdata,
				bool *nulls, bool tupleIsAlive __attribute__((unused)),	void *state)
{
	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_DECLARE;

	BMBuildState *bstate = (BMBuildState *) state;

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_ENTER;

	_bitmap_buildinsert(index, *tupleId, attdata, nulls, bstate);
	bstate->ituples += 1;

	if (((int)bstate->ituples) % 1000 == 0)
		CHECK_FOR_INTERRUPTS();

	MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_EXIT;

}

/*
 * free the memory associated with the stream
 */

static void
stream_free(StreamNode *self)
{
	IndexStream *is = self;
	BMStreamOpaque *so = (BMStreamOpaque *)is->opaque;

	/* opaque may be NULL */
	if (so)
	{
		IndexScanDesc scan = so->scan;
		BMScanOpaque s = (BMScanOpaque)scan->opaque;

		is->opaque = NULL;
		if(s->bm_currPos)
		{
			cleanup_pos(s->bm_currPos);
			pfree(s->bm_currPos);
			s->bm_currPos = NULL;
		}
		if(s->bm_markPos)
		{
			cleanup_pos(s->bm_markPos);
			pfree(s->bm_markPos);
			s->bm_markPos = NULL;
		}

		if (so->entry != NULL)
			pfree(so->entry);

		pfree(s);
		pfree(scan);
		pfree(so);
	}
}

/*
 * free the memory associated with an IndexStream
 */
static void
indexstream_free(StreamNode *self) {
	stream_free(self);

	pfree(self);
}

static void
cleanup_pos(BMScanPosition pos) 
{
	if (pos->nvec == 0)
		return;
	
	/*
	 * Only cleanup bm_batchWords if we have more than one vector since
	 * _bitmap_cleanup_scanpos() will clean it up for the single vector
	 * case.
	 */
	if (pos->nvec > 1)
		 _bitmap_cleanup_batchwords(pos->bm_batchWords);
	_bitmap_cleanup_scanpos(pos->posvecs, pos->nvec);
}


/*
 * pull the next block of tids from a bitmap stream
 */

static bool 
pull_stream(StreamNode *self, PagetableEntry *e)
{
	StreamNode 	   *n = self;
	bool			res = false;
	bool 			newentry = true;
	IndexStream    *is = (IndexStream *)n;
	PagetableEntry *next;
	BMScanPosition	scanPos;
	IndexScanDesc	scan;
	BMStreamOpaque *so;

	so = (BMStreamOpaque *)is->opaque;
	/* empty bitmap vector */
	if(so == NULL)
		return false;
	next = so->entry;

	/* have we already got an entry? */
	if(next && is->nextblock <= next->blockno)
	{
		memcpy(e, next, sizeof(PagetableEntry));
		return true;
	}
	else if (so->is_done)
	{
		stream_free(n);
		is->opaque = NULL;
		return false;
	}
	
	MemSet(e, 0, sizeof(PagetableEntry));

	scan = so->scan;
	scanPos = ((BMScanOpaque)scan->opaque)->bm_currPos;
	e->blockno = is->nextblock;

	so->is_done = false;

	while(true)
	{
		bool found;

		CHECK_FOR_INTERRUPTS();
		
		if (scanPos != NULL)
			res = _bitmap_nextbatchwords(scan, ForwardScanDirection);
		else
			/* we should be initialised! */
			elog(ERROR, "scan position uninitialized");

		found = words_get_match(scanPos->bm_batchWords, &(scanPos->bm_result),
							   is->nextblock, e, newentry);

		if(found)
		{
			res = true;
			break;
		}
		else
		{
			/* Are there any more words available from the index itself? */
			if(!res)
			{
				so->is_done = true;
				res = true;
				break;
			}
			else
			{
				/*
				 * We didn't have enough words to match the whole page, so
				 * tell words_get_match() to continue looking at the page
				 * it finished at
				 */
				is->nextblock = e->blockno;
				newentry = false;
			}
		}
	}

	/*
	 * Set the next block number. We want to skip those blocks that do not
	 * contain possible query results, since in AO index cases, this range
	 * can be very large.
	 */
	is->nextblock = e->blockno + 1;
	if (scanPos->bm_result.nextTid / BM_MAX_TUPLES_PER_PAGE > e->blockno + 1)
		is->nextblock = scanPos->bm_result.nextTid / BM_MAX_TUPLES_PER_PAGE;
	if (so->entry == NULL)
		so->entry = (PagetableEntry *) palloc(sizeof(PagetableEntry));
	memcpy(so->entry, e, sizeof(PagetableEntry));

	return res;
}

/*
 * Make a copy of an index scan descriptor as well as useful fields in
 * the opaque structure
 */

static IndexScanDesc
copy_scan_desc(IndexScanDesc scan)
{
	IndexScanDesc s;
	BMScanOpaque so;
	BMScanPosition sp;
	BMScanPosition spcopy;
	BMBatchWords *w;
	BMVector bsp;

	/* we only need a few fields */
	s = (IndexScanDesc)palloc0(sizeof(IndexScanDescData));
	s->opaque = palloc(sizeof(BMScanOpaqueData));
	spcopy = palloc0(sizeof(BMScanPositionData));
	w = (BMBatchWords *)palloc(sizeof(BMBatchWords));

	s->indexRelation = scan->indexRelation;
	so = (BMScanOpaque)scan->opaque;
	sp = so->bm_currPos;

	if(sp)
	{
		int vec;

		spcopy->done = sp->done;
		spcopy->nvec = sp->nvec;
		spcopy->bm_batchWords = w;

		/* now the batch words */
		w->maxNumOfWords = sp->bm_batchWords->maxNumOfWords;
		w->nwordsread = sp->bm_batchWords->nwordsread;
		w->nextread = sp->bm_batchWords->nextread;
		w->firstTid = sp->bm_batchWords->firstTid;
		w->startNo = sp->bm_batchWords->startNo;
		w->nwords = sp->bm_batchWords->nwords;

		/* the actual words now */
		/* use copy */
	    w->hwords = palloc0(sizeof(BM_HRL_WORD) * 
					BM_CALC_H_WORDS(sp->bm_batchWords->maxNumOfWords));
    	w->cwords = palloc0(sizeof(BM_HRL_WORD) * 
					sp->bm_batchWords->maxNumOfWords);

		memcpy(w->hwords, sp->bm_batchWords->hwords,
			BM_CALC_H_WORDS(sp->bm_batchWords->maxNumOfWords) * sizeof(BM_HRL_WORD));
		memcpy(w->cwords, sp->bm_batchWords->cwords,
			sp->bm_batchWords->maxNumOfWords * sizeof(BM_HRL_WORD));

		memcpy(&spcopy->bm_result, &sp->bm_result, sizeof(BMIterateResult));

		bsp = (BMVector)palloc(sizeof(BMVectorData) * sp->nvec);
		spcopy->posvecs = bsp;
		if(sp->nvec == 1)
		{
			bsp->bm_lovBuffer = sp->posvecs->bm_lovBuffer;
			bsp->bm_lovOffset = sp->posvecs->bm_lovOffset;
			bsp->bm_nextBlockNo = sp->posvecs->bm_nextBlockNo;
			bsp->bm_readLastWords = sp->posvecs->bm_readLastWords;
			bsp->bm_batchWords = w;
		}
		else
		{
			for (vec = 0; vec < sp->nvec; vec++)
			{
				BMVector bmScanPos = &(bsp[vec]);
				BMVector spp = &(sp->posvecs[vec]);

				bmScanPos->bm_lovBuffer = spp->bm_lovBuffer;
				bmScanPos->bm_lovOffset = spp->bm_lovOffset;
				bmScanPos->bm_nextBlockNo = spp->bm_nextBlockNo;
				bmScanPos->bm_readLastWords = spp->bm_readLastWords;

				bmScanPos->bm_batchWords = 
					(BMBatchWords *) palloc0(sizeof(BMBatchWords));
				_bitmap_init_batchwords(bmScanPos->bm_batchWords,
									BM_NUM_OF_HRL_WORDS_PER_PAGE,
									CurrentMemoryContext);
				_bitmap_copy_batchwords(spp->bm_batchWords,
									bmScanPos->bm_batchWords);

			}
		}
	}
	else
		spcopy = NULL;

	((BMScanOpaque)s->opaque)->bm_currPos = spcopy;
	((BMScanOpaque)s->opaque)->bm_markPos = NULL;

	return s;
}

/*
 * Given a set of bitmap words and our current position, get the next
 * page with matches on it.
 *
 * If newentry is false, we're calling the function with a partially filled
 * page table entry. Otherwise, the entry is empty.
 */

static bool
words_get_match(BMBatchWords *words, BMIterateResult *result,
				BlockNumber blockno, PagetableEntry *entry, bool newentry)
{
	tbm_bitmapword newWord;
	int nhrlwords = (TBM_BITS_PER_BITMAPWORD/BM_HRL_WORD_SIZE);
	int hrlwordno;
	int newwordno;
	uint64 start, end;

restart:
	/* compute the first and last tid location for 'blockno' */
	start = ((uint64)blockno) * BM_MAX_TUPLES_PER_PAGE + 1;
	end = ((uint64)(blockno + 1)) * BM_MAX_TUPLES_PER_PAGE;
	newwordno = 0;
	hrlwordno = 0;

	/* If we have read past the requested block, simple return true. */
	if (result->nextTid > end)
		return true;

	if (result->lastScanWordNo >= words->maxNumOfWords)
	{
		result->lastScanWordNo = 0;
		if (result->nextTid < end)
			return false;
		
		else
			return true;
	}
		
	/*
	 * XXX: We assume that BM_HRL_WORD_SIZE is not greater than
	 * TBM_BITS_PER_BITMAPWORD for tidbitmap.
	 */
	Assert(BM_HRL_WORD_SIZE <= TBM_BITS_PER_BITMAPWORD);
	Assert(nhrlwords >= 1); 
	Assert((result->nextTid - start) % BM_HRL_WORD_SIZE == 0);

	/*
	 * find the first tid location in 'words' that is equal to
	 * 'start'.
	 */
	while (words->nwords > 0 && result->nextTid < start)
	{
		BM_HRL_WORD word = words->cwords[result->lastScanWordNo];

		if (IS_FILL_WORD(words->hwords, result->lastScanWordNo))
		{
			uint64	fillLength;
			
			if (word == 0)
				fillLength = 1;
			else
				fillLength = FILL_LENGTH(word);

			if (GET_FILL_BIT(word) == 1)
			{
				if (start - result->nextTid >= fillLength * BM_HRL_WORD_SIZE)
				{
					result->nextTid += fillLength * BM_HRL_WORD_SIZE;
					result->lastScanWordNo++;
					words->nwords--;
				}
				else
				{
					words->cwords[result->lastScanWordNo] -=
						(start - result->nextTid)/BM_HRL_WORD_SIZE;
					result->nextTid = start;
				}
			}
			else
			{
				/*
				 * This word represents compressed non-matches. If it
				 * is sufficiently large, we might be able to skip over a 
				 * large range of blocks which would have no matches
				 */
				result->lastScanWordNo++;
				words->nwords--;
				
				if(fillLength * BM_HRL_WORD_SIZE > end - result->nextTid)
				{
					result->nextTid += fillLength * BM_HRL_WORD_SIZE;
					blockno = result->nextTid / BM_MAX_TUPLES_PER_PAGE;
					goto restart;
				}
				result->nextTid += fillLength * BM_HRL_WORD_SIZE;
			}
		}
		else
		{
			result->nextTid += BM_HRL_WORD_SIZE;
			result->lastScanWordNo++;
			words->nwords--;
		}
	}

	/*
	 * if there are no such a bitmap in the given batch words, then
	 * return false.
	 */
	if (words->nwords == 0)
	{
		result->lastScanWordNo = 0;
		return false;
	}

	if (IS_FILL_WORD(words->hwords, result->lastScanWordNo) &&
		GET_FILL_BIT(words->cwords[result->lastScanWordNo]) == 0)
	{
		uint64 filllen;
		BM_HRL_WORD word = words->cwords[result->lastScanWordNo];

		if(word == 0)
			filllen = 1;
		else
			filllen = FILL_LENGTH(word);

		/*
		 * Check if the fill word would take us past the end of the block
		 * we're currently interested in.
		 */
		if(filllen * BM_HRL_WORD_SIZE > end - result->nextTid)
		{
			result->nextTid += filllen * BM_HRL_WORD_SIZE;
			blockno = result->nextTid / BM_MAX_TUPLES_PER_PAGE;
			result->lastScanWordNo++;
			words->nwords--;
			
			if(newentry)
				goto restart;
			else
				return true;
		}
	}

	/* copy the bitmap for tuples in the given heap page. */
	newWord = 0;
	hrlwordno = ((result->nextTid - start) / BM_HRL_WORD_SIZE) % nhrlwords;
	newwordno = (result->nextTid - start) / TBM_BITS_PER_BITMAPWORD;
	while (words->nwords > 0 && result->nextTid < end)
	{
		BM_HRL_WORD word = words->cwords[result->lastScanWordNo];

		if (IS_FILL_WORD(words->hwords, result->lastScanWordNo))
		{
			if (GET_FILL_BIT(word) == 1)
			{
				newWord |= ((tbm_bitmapword)(LITERAL_ALL_ONE)) <<
					(hrlwordno * BM_HRL_WORD_SIZE);
			}
					
			words->cwords[result->lastScanWordNo]--;
			if (FILL_LENGTH(words->cwords[result->lastScanWordNo]) == 0)
			{
				result->lastScanWordNo++;
				words->nwords--;
			}
		}
		else
		{
			newWord |= ((tbm_bitmapword)word) << (hrlwordno * BM_HRL_WORD_SIZE);
			result->lastScanWordNo++;
			words->nwords--;
		}

		hrlwordno = (hrlwordno + 1) % nhrlwords;
		result->nextTid += BM_HRL_WORD_SIZE;

		if (hrlwordno % nhrlwords == 0)
		{
			Assert(newwordno < WORDS_PER_PAGE || newwordno < WORDS_PER_CHUNK);

			entry->words[newwordno] |= newWord;
			newwordno++;

			/* reset newWord */
			newWord = 0;
		}
	}

	if (hrlwordno % nhrlwords != 0)
	{
		Assert(newwordno < WORDS_PER_PAGE || newwordno < WORDS_PER_CHUNK);
		entry->words[newwordno] |= newWord;
	}

	entry->blockno = blockno;
	if (words->nwords == 0)
	{
		result->lastScanWordNo = 0;

		if (result->nextTid < end)
			return false;
	}

	return true;
}

/*
 * GetBitmapIndexAuxOids - Given an open index, fetch and return the oids for
 * the bitmap subobjects (pg_bm_xxxx + pg_bm_xxxx_index).
 *
 * Note: Currently this information is not stored directly in the catalog, but
 * is hidden away inside the metadata page of the index.  Future versions should
 * move this information into the catalog.
 */
void 
GetBitmapIndexAuxOids(Relation index, Oid *heapId, Oid *indexId)
{
	MIRROREDLOCK_BUFMGR_DECLARE;

	Buffer     metabuf;
	BMMetaPage metapage;
	

	/* Only Bitmap Indexes have bitmap related sub-objects */
	if (!RelationIsBitmapIndex(index))
	{
		*heapId = InvalidOid;
		*indexId = InvalidOid;
		return;
	}
		
	// -------- MirroredLock ----------
	MIRROREDLOCK_BUFMGR_LOCK;
	
	metabuf = _bitmap_getbuf(index, BM_METAPAGE, BM_READ);
	metapage = _bitmap_get_metapage_data(index, metabuf);

	*heapId  = metapage->bm_lov_heapId;
	*indexId = metapage->bm_lov_indexId;

	_bitmap_relbuf(metabuf);
	
	MIRROREDLOCK_BUFMGR_UNLOCK;
	// -------- MirroredLock ----------
	
}

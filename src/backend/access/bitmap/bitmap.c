/*-------------------------------------------------------------------------
 *
 * bitmap.c
 *	  Implementation of the Hybrid Run-Length (HRL) on-disk bitmap index.
 *
 * Portions Copyright (c) 2007-2010 Greenplum Inc
 * Portions Copyright (c) 2010-2012 EMC Corporation
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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

#include "access/amapi.h"
#include "access/amvalidate.h"
#include "access/genam.h"
#include "access/bitmap.h"
#include "access/bitmap_private.h"
#include "access/reloptions.h"
#include "access/nbtree.h"		/* for btree_or_bitmap_validate() */
#include "access/tableam.h"
#include "access/xact.h"
#include "catalog/index.h"
#include "catalog/pg_am.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_opclass.h"
#include "miscadmin.h"
#include "nodes/tidbitmap.h"
#include "storage/lmgr.h"
#include "storage/smgr.h"
#include "parser/parse_oper.h"
#include "utils/memutils.h"
#include "utils/index_selfuncs.h"
#include "utils/syscache.h"

#include "nodes/execnodes.h"

static void bmbuildCallback(Relation index,	ItemPointer tupleId, Datum *attdata,
							bool *nulls, bool tupleIsAlive,	void *state);
static bool words_get_match(BMBatchWords *words, BMIterateResult *result,
                            BlockNumber blockno, PagetableEntry *entry,
							bool newentry);
static IndexScanDesc copy_scan_desc(IndexScanDesc scan);
static void indexstream_free(StreamNode *self);
static bool pull_stream(StreamBMIterator *iterator, PagetableEntry *e);
static void cleanup_pos(BMScanPosition pos);

/* type to hide BM specific stream state */
typedef struct BMStreamOpaque
{
	IndexScanDesc scan;
	PagetableEntry *entry;
	/* Indicate that this stream contains no more bitmap words. */
	bool is_done;
} BMStreamOpaque;

static void stream_free(BMStreamOpaque *so);

/*
 * Bitmap index handler function: return IndexAmRoutine with access method parameters
 * and callbacks.
 */
Datum
bmhandler(PG_FUNCTION_ARGS)
{
	IndexAmRoutine *amroutine = makeNode(IndexAmRoutine);

	/* these are mostly the same as B-tree */
	amroutine->amstrategies = BTMaxStrategyNumber;
	amroutine->amsupport = BTNProcs;
	amroutine->amcanorder = false;
	amroutine->amcanorderbyop = false;
	amroutine->amcanbackward = false;
	amroutine->amcanunique = true;
	amroutine->amcanmulticol = true;
	amroutine->amoptionalkey = true;
	amroutine->amsearcharray = false;
	amroutine->amsearchnulls = false;
	amroutine->amstorage = false;
	amroutine->amclusterable = false;
	amroutine->ampredlocks = false;
	amroutine->amkeytype = InvalidOid;

	amroutine->ambuild = bmbuild;
	amroutine->ambuildempty = bmbuildempty;
	amroutine->aminsert = bminsert;
	amroutine->ambulkdelete = bmbulkdelete;
	amroutine->amvacuumcleanup = bmvacuumcleanup;
	amroutine->amcanreturn = NULL;
	amroutine->amcostestimate = bmcostestimate;
	amroutine->amoptions = bmoptions;
	amroutine->amproperty = NULL;
	amroutine->amvalidate = bmvalidate;
	amroutine->ambeginscan = bmbeginscan;
	amroutine->amrescan = bmrescan;
	amroutine->amgettuple = bmgettuple;
	amroutine->amgetbitmap = bmgetbitmap;
	amroutine->amendscan = bmendscan;
	amroutine->ammarkpos = bmmarkpos;
	amroutine->amrestrpos = bmrestrpos;

	PG_RETURN_POINTER(amroutine);
}

/*
 * bmbuild() -- Build a new bitmap index.
 */
IndexBuildResult *
bmbuild(Relation heap, Relation index, IndexInfo *indexInfo)
{
	double      reltuples;
	BMBuildState bmstate;
	IndexBuildResult *result;
	TupleDesc	tupDesc;

	if (indexInfo->ii_Concurrent)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("CONCURRENTLY is not supported when creating bitmap indexes")));

	/* We expect this to be called exactly once. */
	if (RelationGetNumberOfBlocks(index) != 0)
		ereport (ERROR,
				(errcode(ERRCODE_INDEX_CORRUPTED),
				errmsg("index \"%s\" already contains data",
				RelationGetRelationName(index))));

	tupDesc = RelationGetDescr(index);

	/* initialize the bitmap index for MAIN_FORKNUM. */
	_bitmap_init(index, RelationNeedsWAL(index), false);

	/* initialize the build state. */
	_bitmap_init_buildstate(index, &bmstate);

	/* do the heap scan */
	reltuples = table_index_build_scan(heap, index, indexInfo, true, true,
									   bmbuildCallback, (void *) &bmstate,
									   NULL);
	/* clean up the build state */
	_bitmap_cleanup_buildstate(index, &bmstate);
	
	/* return statistics */
	result = (IndexBuildResult *) palloc0(sizeof(IndexBuildResult));

	result->heap_tuples = reltuples;
	result->index_tuples = bmstate.ituples;

	return result;
}

/*
 *	bmbuildempty() -- build an empty bitmap index in the initialization fork
 */
void
bmbuildempty(Relation indexrel)
{
	/* initialize meta page and first LOV page for INIT_FORKNUM */
	_bitmap_init(indexrel, true, true);
}

/*
 * bminsert() -- insert an index tuple into a bitmap index.
 */
bool
bminsert(Relation rel, Datum *values, bool *isnull,
		 ItemPointer ht_ctid, Relation heapRel,
		 IndexUniqueCheck checkUnique,
		 IndexInfo *indexInfo)
{
	_bitmap_doinsert(rel, *ht_ctid, values, isnull);
	return true;
}

/*
 * bmgettuple() -- return the next tuple in a scan.
 */
bool
bmgettuple(IndexScanDesc scan, ScanDirection dir)
{
	BMScanOpaque  so = (BMScanOpaque) scan->opaque;
	bool		res;

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

	return res;
}

static void
stream_end_iterate(StreamBMIterator *self)
{
	/* opaque may be NULL */
	if (self->opaque) {
		stream_free(self->opaque);
		self->opaque = NULL;
	}
}

static void
stream_begin_iterate(StreamNode *self, StreamBMIterator *iterator)
{
	BMStreamOpaque *so;
	IndexScanDesc scan = self->opaque;

	iterator->pull = pull_stream;
	iterator->end_iterate = stream_end_iterate;

	if (scan == NULL)
	{
		iterator->opaque = NULL;
	}
	else
	{
		so = palloc(sizeof(BMStreamOpaque));
		so->scan = copy_scan_desc(scan);
		so->entry = NULL;
		so->is_done = false;

		iterator->opaque = so;
	}
}

/*
 * bmgetbitmap() -- return a stream bitmap.
 */
Node *
bmgetbitmap(IndexScanDesc scan, Node *bm)
{
	/* We ignore the second argument as we're returning a hash bitmap */
	IndexStream	 *is;
	BMScanPosition	scanPos;
	bool res;

	res = _bitmap_firstbatchwords(scan, ForwardScanDirection);

	scanPos = ((BMScanOpaque)scan->opaque)->bm_currPos;
	scanPos->bm_result.nextTid = 1;

	/* perhaps this should be in a special context? */
	is = (IndexStream *)palloc0(sizeof(IndexStream));
	is->type = BMS_INDEX;
	is->begin_iterate = stream_begin_iterate;
	is->free = indexstream_free;
	is->set_instrument = NULL;
	is->upd_instrument = NULL;
	is->opaque = NULL;

	if (res)
	{
		int vec;

		is->opaque = copy_scan_desc(scan);

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

	if (!bm)
	{
		StreamBitmap *sb = makeNode(StreamBitmap);
		sb->streamNode = is;
		bm = (Node *)sb;
	}
	else if (IsA(bm, StreamBitmap))
	{
		stream_add_node((StreamBitmap *)bm, is, BMS_OR);
	}
	else
	{
		elog(ERROR, "non stream bitmap");
	}

	return bm;
}

/*
 * bmbeginscan() -- start a scan on the bitmap index.
 */
IndexScanDesc
bmbeginscan(Relation rel, int nkeys, int norderbys)
{
	IndexScanDesc scan;
	BMScanOpaque	so;

	/* no order by operators allowed */
	Assert(norderbys == 0);

	/* get the scan */
	scan = RelationGetIndexScan(rel, nkeys, norderbys);

	/* allocate private workspace */
	so = (BMScanOpaque) palloc(sizeof(BMScanOpaqueData));
	so->bm_currPos = NULL;
	so->bm_markPos = NULL;
	so->cur_pos_valid = false;
	so->mark_pos_valid = false;

	scan->xs_itupdesc = RelationGetDescr(rel);

	scan->opaque = so;

	return scan;
}

/*
 * bmrescan() -- restart a scan on the bitmap index.
 */
void
bmrescan(IndexScanDesc scan, ScanKey scankey, int nscankeys,
		 ScanKey orderbys, int norderbys)
{
	BMScanOpaque	so = (BMScanOpaque) scan->opaque;

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
}

/*
 * bmendscan() -- close a scan.
 */
void
bmendscan(IndexScanDesc scan)
{
	BMScanOpaque	so = (BMScanOpaque) scan->opaque;

	/* free the space */
	if (so->bm_currPos != NULL)
	{
		/*
		 * release the buffers that have been stored for each related 
		 * bitmap vector.
		 */
		cleanup_pos(so->bm_currPos);
		pfree(so->bm_currPos);
		so->bm_currPos = NULL;
	}

	if (so->bm_markPos != NULL)
	{
		cleanup_pos(so->bm_markPos);
		pfree(so->bm_markPos);
		so->bm_markPos = NULL;
	}

	pfree(so);
	scan->opaque = NULL;
}

/*
 * bmmarkpos() -- save the current scan position.
 */
void
bmmarkpos(IndexScanDesc scan)
{
	BMScanOpaque	so = (BMScanOpaque) scan->opaque;
	BMVector	bmScanPos;
	uint32 vectorNo;

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
}

/*
 * bmrestrpos() -- restore a scan to the last saved position.
 */
void
bmrestrpos(IndexScanDesc scan)
{
	BMScanOpaque	so = (BMScanOpaque) scan->opaque;

	BMVector	bmScanPos;
	uint32 vectorNo;

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
}

/*
 * bmbulkdelete() -- bulk delete index entries
 *
 * Re-index is performed before retrieving the number of tuples
 * indexed in this index.
 */
IndexBulkDeleteResult *
bmbulkdelete(IndexVacuumInfo *info,
			 IndexBulkDeleteResult *stats,
			 IndexBulkDeleteCallback callback,
			 void *callback_state)
{
	Relation	rel = info->index;

	/* allocate stats if first time through, else re-use existing struct */
	if (stats == NULL)
		stats = (IndexBulkDeleteResult *)
			palloc0(sizeof(IndexBulkDeleteResult));	

	reindex_index(RelationGetRelid(rel), true, rel->rd_rel->relpersistence, 0);

	CommandCounterIncrement();

	stats->num_pages = RelationGetNumberOfBlocks(rel);
	/* Since we re-build the index, set this to number of heap tuples. */
	stats->num_index_tuples = info->num_heap_tuples;
	stats->tuples_removed = 0;

	return stats;
}

/*
 * bmvacuumcleanup() -- post-vacuum cleanup.
 *
 * We do nothing useful here.
 */
IndexBulkDeleteResult *
bmvacuumcleanup(IndexVacuumInfo *info,
				IndexBulkDeleteResult *stats)
{
	Relation	rel = info->index;

	if(stats == NULL)
		stats = (IndexBulkDeleteResult *)palloc0(sizeof(IndexBulkDeleteResult));

	/* update statistics */
	stats->num_pages = RelationGetNumberOfBlocks(rel);
	stats->pages_deleted = 0;
	stats->pages_free = 0;
	/* XXX: dodgy hack to shutup index_scan() and vacuum_index() */
	stats->num_index_tuples = info->num_heap_tuples;

	return stats;
}

/*
 * Per-tuple callback from IndexBuildHeapScan
 */
static void
bmbuildCallback(Relation index, ItemPointer tupleId, Datum *attdata,
				bool *nulls, bool tupleIsAlive pg_attribute_unused(),	void *state)
{
	BMBuildState *bstate = (BMBuildState *) state;

	_bitmap_buildinsert(index, *tupleId, attdata, nulls, bstate);
	bstate->ituples += 1;

	if (((int)bstate->ituples) % 1000 == 0)
		CHECK_FOR_INTERRUPTS();
}

/*
 * Free an IndexScanDesc created by copy_scan_desc(). If releaseBuffers is true,
 * any Buffers pointed to by the BMScanPositions will be released as well.
 */
static void
free_scan_desc(IndexScanDesc scan, bool releaseBuffers)
{
	BMScanOpaque s = scan->opaque;
	int vec;

	if (s->bm_currPos)
	{
		if (!releaseBuffers)
		{
			for (vec = 0; vec < s->bm_currPos->nvec; vec++)
			{
				BMVector bmvec = &(s->bm_currPos->posvecs[vec]);
				bmvec->bm_lovBuffer = InvalidBuffer;
			}
		}

		cleanup_pos(s->bm_currPos);
		pfree(s->bm_currPos);
		s->bm_currPos = NULL;
	}
	if (s->bm_markPos)
	{
		if (!releaseBuffers)
		{
			for (vec = 0; vec < s->bm_markPos->nvec; vec++)
			{
				BMVector bmvec = &(s->bm_markPos->posvecs[vec]);
				bmvec->bm_lovBuffer = InvalidBuffer;
			}
		}

		cleanup_pos(s->bm_markPos);
		pfree(s->bm_markPos);
		s->bm_markPos = NULL;
	}

	pfree(s);
	pfree(scan);
}

/*
 * free the memory associated with the stream
 */

static void
stream_free(BMStreamOpaque *so)
{
	/* opaque may be NULL */
	if (so)
	{
		free_scan_desc(so->scan, false /* we can't release the underlying
										  Buffers yet as there may be other
										  iterators in operation */);

		if (so->entry != NULL)
			pfree(so->entry);

		pfree(so);
	}
}

/*
 * free the memory associated with an IndexStream
 */
static void
indexstream_free(StreamNode *self) {
	IndexScanDesc scan = self->opaque;
	if (scan)
		free_scan_desc(scan, true /* we can release the scanned Buffers now */);
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
	{
		_bitmap_cleanup_batchwords(pos->bm_batchWords);
		if (pos->bm_batchWords != NULL)
			pfree(pos->bm_batchWords);
	}
	_bitmap_cleanup_scanpos(pos->posvecs, pos->nvec);
}


/*
 * pull the next block of tids from a bitmap stream
 */

static bool 
pull_stream(StreamBMIterator *iterator, PagetableEntry *e)
{
	bool			res = false;
	bool 			newentry = true;
	PagetableEntry *next;
	BMScanPosition	scanPos;
	IndexScanDesc	scan;
	BMStreamOpaque *so = iterator->opaque;

	/* empty bitmap vector */
	if(so == NULL)
		return false;
	next = so->entry;

	/* have we already got an entry? */
	if(next && iterator->nextblock <= next->blockno)
	{
		memcpy(e, next, sizeof(PagetableEntry));
		return true;
	}
	else if (so->is_done)
	{
		/* Just free opaque state early so that we could short circuit. */
		if (iterator->opaque) {
			stream_free(iterator->opaque);
			iterator->opaque = NULL;
		}
		return false;
	}

	scan = so->scan;
	scanPos = ((BMScanOpaque)scan->opaque)->bm_currPos;
	e->blockno = iterator->nextblock;

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
							   iterator->nextblock, e, newentry);

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
				iterator->nextblock = e->blockno;
				newentry = false;
			}
		}
	}

	/*
	 * Set the next block number. We want to skip those blocks that do not
	 * contain possible query results, since in AO index cases, this range
	 * can be very large.
	 */
	iterator->nextblock = e->blockno + 1;
	if (scanPos->bm_result.nextTid / BM_MAX_TUPLES_PER_PAGE > e->blockno + 1)
		iterator->nextblock = scanPos->bm_result.nextTid / BM_MAX_TUPLES_PER_PAGE;
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

	s->indexRelation = scan->indexRelation;
	so = (BMScanOpaque)scan->opaque;
	sp = so->bm_currPos;

	if(sp)
	{
		int vec;

		spcopy = palloc0(sizeof(BMScanPositionData));

		spcopy->done = sp->done;
		spcopy->nvec = sp->nvec;

		/* now the batch words */
		w = (BMBatchWords *)palloc(sizeof(BMBatchWords));
	    w->hwords = palloc0(sizeof(BM_HRL_WORD) * 
					BM_CALC_H_WORDS(sp->bm_batchWords->maxNumOfWords));
    	w->cwords = palloc0(sizeof(BM_HRL_WORD) * 
					sp->bm_batchWords->maxNumOfWords);

		_bitmap_copy_batchwords(sp->bm_batchWords, w);
		spcopy->bm_batchWords = w;

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
	Buffer     metabuf;
	BMMetaPage metapage;
	

	/* Only Bitmap Indexes have bitmap related sub-objects */
	if (!RelationIsBitmapIndex(index))
	{
		*heapId = InvalidOid;
		*indexId = InvalidOid;
		return;
	}
	
	metabuf = _bitmap_getbuf(index, BM_METAPAGE, BM_READ);
	metapage = _bitmap_get_metapage_data(index, metabuf);

	*heapId  = metapage->bm_lov_heapId;
	*indexId = metapage->bm_lov_indexId;

	_bitmap_relbuf(metabuf);
}

bytea *
bmoptions(Datum reloptions, bool validate)
{
	return default_reloptions(reloptions, validate, RELOPT_KIND_BITMAP);
}

/*
 * Ask appropriate access method to validate the specified opclass.
 */
bool
bmvalidate(Oid opclassoid)
{
	/*
	 * Bitmap indexes use the same opclass support functions and strategies
	 * as B-tree indexes. In fact, we use a real B-tree index for the LOV
	 * tree. So borrow B-tree's validate function.
	 */
	return btree_or_bitmap_validate(opclassoid, "bitmap");
}

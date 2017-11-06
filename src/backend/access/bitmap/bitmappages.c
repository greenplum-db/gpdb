/*-------------------------------------------------------------------------
 *
 * bitmappages.c
 *	  Bitmap index page management code for the bitmap index.
 *
 * Portions Copyright (c) 2007-2010 Greenplum Inc
 * Portions Copyright (c) 2010-2012 EMC Corporation
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 2006-2008, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/bitmap/bitmappages.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"

#include "access/genam.h"
#include "access/tupdesc.h"
#include "access/bitmap.h"
#include "parser/parse_oper.h"
#include "storage/lmgr.h"
#include "utils/memutils.h"
#include "utils/lsyscache.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

/* 
 * Helper functions for hashing and matching build data. At this stage, the
 * hash API doesn't know about complex keys like those use during index
 * creation (the key is an array of key attributes). c.f. execGrouping.c.
 */
typedef struct BMBuildHashData
{
	int			natts;
	FmgrInfo   *hash_funcs;
	bool       *hash_func_is_strict;
	FmgrInfo   *eq_funcs;
	MemoryContext tmpcxt;
	MemoryContext hash_cxt;
} BMBuildHashData;


static BMBuildHashData *cur_bmbuild = NULL;

static uint32 build_hash_key(const void *key, Size keysize);
static int build_match_key(const void *key1, const void *key2, Size keysize);
static void *build_keycopy(void *dest, const void *src, Size keysize);
/*
 * _bitmap_getbuf() -- return the buffer for the given block number and
 * 					   the access method.
 */
Buffer
_bitmap_getbuf(Relation rel, BlockNumber blkno, int access)
{
	Buffer buf;

	MIRROREDLOCK_BUFMGR_MUST_ALREADY_BE_HELD;

	if (blkno != P_NEW)
	{
		buf = ReadBuffer(rel, blkno);
		if (access != BM_NOLOCK)
			LockBuffer(buf, access);
	}

	else
	{
		bool		needLock;

		Assert(access == BM_WRITE);

		/*
		 * Extend the relation by one page.
		 *
		 * We have to use a lock to ensure no one else is extending the rel at
		 * the same time, else we will both try to initialize the same new
		 * page.  We can skip locking for new or temp relations, however,
		 * since no one else could be accessing them.
		 */
		needLock = !RELATION_IS_LOCAL(rel);

		if (needLock)
			LockRelationForExtension(rel, ExclusiveLock);

		buf = ReadBuffer(rel, P_NEW);

		/* Acquire buffer lock on new page */
		LockBuffer(buf, BM_WRITE);

		/*
		 * Release the file-extension lock; it's now OK for someone else to
		 * extend the relation some more.
		 */
		if (needLock)
			UnlockRelationForExtension(rel, ExclusiveLock);
	}

	return buf;
}

/*
 * _bitmap_wrtbuf() -- write a buffer page to disk.
 *
 * Release the lock and the pin held on the buffer.
 */
void
_bitmap_wrtbuf(Buffer buf)
{
	MIRROREDLOCK_BUFMGR_MUST_ALREADY_BE_HELD;

	MarkBufferDirty(buf);
	UnlockReleaseBuffer(buf);
}

/*
 * _bitmap_relbuf() -- release the buffer without writing.
 */
void
_bitmap_relbuf(Buffer buf)
{
	MIRROREDLOCK_BUFMGR_MUST_ALREADY_BE_HELD;

	UnlockReleaseBuffer(buf);
}

/*
 * _bitmap_init_lovpage -- initialize a new LOV page.
 */
void
_bitmap_init_lovpage(Relation rel __attribute__((unused)), Buffer buf)
{
	Page			page;

	page = (Page) BufferGetPage(buf);

	if(PageIsNew(page))
		PageInit(page, BufferGetPageSize(buf), 0);
}

/*
 * _bitmap_init_bitmappage() -- initialize a new page to store the bitmap.
 */
void
_bitmap_init_bitmappage(Relation rel __attribute__((unused)), Buffer buf)
{
	Page			page;
	BMBitmapOpaque	opaque;

	page = (Page) BufferGetPage(buf);

	if(PageIsNew(page))
		PageInit(page, BufferGetPageSize(buf), sizeof(BMBitmapOpaqueData));

	/* even though page may not be new, reset all values */
	opaque = (BMBitmapOpaque) PageGetSpecialPointer(page);
	opaque->bm_hrl_words_used = 0;
	opaque->bm_bitmap_next = InvalidBlockNumber;
	opaque->bm_last_tid_location = 0;
}

/*
 * _bitmap_init_buildstate() -- initialize the build state before building
 *	a bitmap index.
 */
void
_bitmap_init_buildstate(Relation index, BMBuildState *bmstate)
{
	MIRROREDLOCK_BUFMGR_DECLARE;

	BMMetaPage	mp;
	HASHCTL		hash_ctl;
	int			hash_flags;
	int			i;
	Buffer		metabuf;


	/* initialize the build state */
	bmstate->bm_tupDesc = RelationGetDescr(index);
	bmstate->bm_tidLocsBuffer = (BMTidBuildBuf *)
		palloc(sizeof(BMTidBuildBuf));
	bmstate->bm_tidLocsBuffer->byte_size = 0;
	bmstate->bm_tidLocsBuffer->lov_blocks = NIL;
	bmstate->bm_tidLocsBuffer->max_lov_block = InvalidBlockNumber;
	
	// -------- MirroredLock ----------
	MIRROREDLOCK_BUFMGR_LOCK;
	
	metabuf = _bitmap_getbuf(index, BM_METAPAGE, BM_READ);
	mp = _bitmap_get_metapage_data(index, metabuf);
	_bitmap_open_lov_heapandindex(index, mp, &(bmstate->bm_lov_heap),
								  &(bmstate->bm_lov_index), 
								  RowExclusiveLock);

	_bitmap_relbuf(metabuf);
	
	MIRROREDLOCK_BUFMGR_UNLOCK;
	// -------- MirroredLock ----------
	
	cur_bmbuild = (BMBuildHashData *)palloc(sizeof(BMBuildHashData));
	cur_bmbuild->hash_funcs = (FmgrInfo *)
						palloc(sizeof(FmgrInfo) * bmstate->bm_tupDesc->natts);
	cur_bmbuild->eq_funcs = (FmgrInfo *)
                        palloc(sizeof(FmgrInfo) * bmstate->bm_tupDesc->natts);
    cur_bmbuild->hash_func_is_strict = (bool *)
                        palloc(sizeof(bool) * bmstate->bm_tupDesc->natts);

	for (i = 0; i < bmstate->bm_tupDesc->natts; i++)
	{
		Oid			typid = bmstate->bm_tupDesc->attrs[i]->atttypid;
		Operator	optup;
		Oid			eq_opr;
		Oid			eq_function;
		Oid			left_hash_function;
		Oid			right_hash_function;

		optup = equality_oper(typid, false);
		eq_opr = oprid(optup);
		eq_function = oprfuncid(optup);
		ReleaseSysCache(optup);

		if (!get_op_hash_functions(eq_opr,
								   &left_hash_function,
								   &right_hash_function))
		{
			pfree(cur_bmbuild);
			cur_bmbuild = NULL;
			break;
		}

		Assert(left_hash_function == right_hash_function);
		fmgr_info(eq_function, &cur_bmbuild->eq_funcs[i]);
		fmgr_info(right_hash_function, &cur_bmbuild->hash_funcs[i]);
        cur_bmbuild->hash_func_is_strict[i] = func_strict(right_hash_function);
	}

	if (cur_bmbuild)
	{
		cur_bmbuild->natts = bmstate->bm_tupDesc->natts;
		cur_bmbuild->tmpcxt = AllocSetContextCreate(CurrentMemoryContext,
        	                      "Bitmap build temp space",
            	                  ALLOCSET_DEFAULT_MINSIZE,
                	              ALLOCSET_DEFAULT_INITSIZE,
                    	          ALLOCSET_DEFAULT_MAXSIZE);

		/* setup the hash table */
	    MemSet(&hash_ctl, 0, sizeof(hash_ctl));

	    /**
	     * Reserve enough space for the hash key header and then the data segments (values followed by nulls)
	     */
    	hash_ctl.keysize = MAXALIGN(sizeof(BMBuildHashKey)) +
                           MAXALIGN(sizeof(Datum) * cur_bmbuild->natts) +
                           MAXALIGN(sizeof(bool) * cur_bmbuild->natts);

		hash_ctl.entrysize = hash_ctl.keysize + sizeof(BMBuildLovData) + 200; 
    	hash_ctl.hash = build_hash_key;
	    hash_ctl.match = build_match_key;
	    hash_ctl.keycopy = build_keycopy;
    	hash_ctl.hcxt = AllocSetContextCreate(CurrentMemoryContext,
        	                      "Bitmap build hash table",
            	                  ALLOCSET_DEFAULT_MINSIZE,
                	              ALLOCSET_DEFAULT_INITSIZE,
                    	          ALLOCSET_DEFAULT_MAXSIZE);
		cur_bmbuild->hash_cxt = hash_ctl.hcxt;

		hash_flags = HASH_ELEM | HASH_FUNCTION | HASH_COMPARE | HASH_CONTEXT | HASH_KEYCOPY;

		bmstate->lovitem_hash = hash_create("Bitmap index build lov item hash",
											100, &hash_ctl, hash_flags);
        bmstate->lovitem_hashKeySize = hash_ctl.keysize;
	}
	else
	{
		int attno;
		bmstate->lovitem_hash = NULL;
		bmstate->lovitem_hashKeySize = 0;
		bmstate->bm_lov_scanKeys =
			(ScanKey)palloc0(bmstate->bm_tupDesc->natts * sizeof(ScanKeyData));

		for (attno = 0; attno < bmstate->bm_tupDesc->natts; attno++)
		{
			RegProcedure	opfuncid;
			Oid				atttypid;

			atttypid = bmstate->bm_tupDesc->attrs[attno]->atttypid;
			opfuncid = equality_oper_funcid(atttypid);

			ScanKeyEntryInitialize(&(bmstate->bm_lov_scanKeys[attno]), SK_ISNULL, 
							   attno + 1, BTEqualStrategyNumber, InvalidOid, 
							   opfuncid, 0);
		}

		bmstate->bm_lov_scanDesc = index_beginscan(bmstate->bm_lov_heap,
							 bmstate->bm_lov_index, GetActiveSnapshot(), 
							 bmstate->bm_tupDesc->natts,
							 bmstate->bm_lov_scanKeys);
	}

	/*
	 * We need to log index creation in WAL iff WAL archiving is enabled
	 * AND it's not a temp index. Currently, since building an index
	 * writes page to the shared buffer, we can't disable WAL archiving.
	 * We will add this shortly.
	 */	
	bmstate->use_wal = !index->rd_istemp;
}

/*
 * _bitmap_cleanup_buildstate() -- clean up the build state after
 *	inserting all rows in the heap into the bitmap index.
 */
void
_bitmap_cleanup_buildstate(Relation index, BMBuildState *bmstate)
{
	/* write out remaining tids in bmstate->bm_tidLicsBuffer */
	BMTidBuildBuf	*tidLocsBuffer = bmstate->bm_tidLocsBuffer;
	_bitmap_write_alltids(index, tidLocsBuffer, bmstate->use_wal);

	pfree(bmstate->bm_tidLocsBuffer);

	if (cur_bmbuild)
	{
		MemoryContextDelete(cur_bmbuild->tmpcxt);
		MemoryContextDelete(cur_bmbuild->hash_cxt);
		pfree(cur_bmbuild->hash_funcs);
		pfree(cur_bmbuild->eq_funcs);
		pfree(cur_bmbuild);
		cur_bmbuild = NULL;
	}
	else
	{
		/* 
		 * We might have build an index on a non-hashable data type, in
		 * which case we will have searched the btree manually. Free associated
		 * memory.
		 */
		index_endscan(bmstate->bm_lov_scanDesc);
		pfree(bmstate->bm_lov_scanKeys);
	}

	_bitmap_close_lov_heapandindex(bmstate->bm_lov_heap,bmstate->bm_lov_index,
						 		   RowExclusiveLock);
}

/*
 * _bitmap_init() -- initialize the bitmap index.
 *
 * Create the meta page, a new heap which stores the distinct values for
 * the attributes to be indexed, a btree index on this new heap for searching
 * those distinct values, and the first LOV page.
 */
void
_bitmap_init(Relation indexrel, bool use_wal)
{
	MIRROREDLOCK_BUFMGR_DECLARE;

	BMMetaPage		metapage;
	Buffer			metabuf;
	Page			page;
	Buffer			buf;
	BMLOVItem 		lovItem;
	OffsetNumber	newOffset;
	Page			currLovPage;
	OffsetNumber	o;
	Oid			lovHeapOid;
	Oid			lovIndexOid;

	/* sanity check */
	if (RelationGetNumberOfBlocks(indexrel) != 0)
		ereport(ERROR,
				(errcode(ERRCODE_INDEX_CORRUPTED),
				errmsg("cannot initialize non-empty bitmap index \"%s\"",
				RelationGetRelationName(indexrel)),
				errSendAlert(true)));
	
	// -------- MirroredLock ----------
	MIRROREDLOCK_BUFMGR_LOCK;
	
	/* create the metapage */
	metabuf = _bitmap_getbuf(indexrel, P_NEW, BM_WRITE);
	page = BufferGetPage(metabuf);
	Assert(PageIsNew(page));

	/* initialize the LOV metadata */
	_bitmap_create_lov_heapandindex(indexrel, &lovHeapOid, &lovIndexOid);

	START_CRIT_SECTION();

	MarkBufferDirty(metabuf);

	/* initialize the metapage */
	PageInit(page, BufferGetPageSize(metabuf), 0);
	metapage = (BMMetaPage) PageGetContents(page);
	
	metapage->bm_magic = BITMAP_MAGIC;
	metapage->bm_version = BITMAP_VERSION;
	metapage->bm_lov_heapId = lovHeapOid;
	metapage->bm_lov_indexId = lovIndexOid;

	if (use_wal)
		_bitmap_log_metapage(indexrel, page);

	/* allocate the first LOV page. */
	buf = _bitmap_getbuf(indexrel, P_NEW, BM_WRITE);
	_bitmap_init_lovpage(indexrel, buf);

	MarkBufferDirty(buf);

	currLovPage = BufferGetPage(buf);

	/* set the first item to support NULL value */
	lovItem = _bitmap_formitem(0);
	newOffset = OffsetNumberNext(PageGetMaxOffsetNumber(currLovPage));

	/*
	 * XXX: perhaps this could be a special page, with more efficient storage
	 * after all, we have fixed size data
	 */
	o = PageAddItem(currLovPage, (Item)lovItem, sizeof(BMLOVItemData),
                    newOffset, false, false);

	if (o == InvalidOffsetNumber)
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("failed to add LOV item to \"%s\"",
				 RelationGetRelationName(indexrel))));

	metapage->bm_lov_lastpage = BufferGetBlockNumber(buf);

	if(use_wal)
		_bitmap_log_lovitem(indexrel, buf, newOffset, lovItem, metabuf, true);

	END_CRIT_SECTION();

	_bitmap_wrtbuf(buf);
	_bitmap_wrtbuf(metabuf);
	
	MIRROREDLOCK_BUFMGR_UNLOCK;
	// -------- MirroredLock ----------
	
	pfree(lovItem);
}

/*
 * Build a hash of the key we're indexing.
 */

static uint32
build_hash_key(const void *key, Size keysize __attribute__((unused)))
{
    Assert(key);

    BMBuildHashKey *keyData = (BMBuildHashKey*)key;
	Datum *k = keyData->attributeValueArr;
	bool *isNull = keyData->isNullArr;

	int i;
	uint32 hashkey = 0;

	for(i = 0; i < cur_bmbuild->natts; i++)
	{
		/* rotate hashkey left 1 bit at each step */
		hashkey = (hashkey << 1) | ((hashkey & 0x80000000) ? 1 : 0);

        if ( isNull[i] && cur_bmbuild->hash_func_is_strict[i])
        {
            /* leave hashkey unmodified, equivalent to hashcode 0 */
        }
        else
        {
            hashkey ^= DatumGetUInt32(FunctionCall1(&cur_bmbuild->hash_funcs[i], k[i]));
        }

	}
	return hashkey;
}

/*
 * Test whether key1 matches key2. Since the equality functions may leak,
 * reset the temporary context at each call and do all equality calculation
 * in that context.
 */
static int
build_match_key(const void *key1, const void *key2, Size keysize __attribute__((unused)))
{
    Assert(key1);
    Assert(key2);

    BMBuildHashKey *keyData1 = (BMBuildHashKey*)key1;
	Datum *k1 = keyData1->attributeValueArr;
	bool *isNull1 = keyData1->isNullArr;
	
	BMBuildHashKey *keyData2 = (BMBuildHashKey*)key2;
	Datum *k2 = keyData2->attributeValueArr;
	bool *isNull2 = keyData2->isNullArr;
	
    int numKeys = cur_bmbuild->natts;

	int i;
	MemoryContext old;
	int result = 0;

	MemoryContextReset(cur_bmbuild->tmpcxt);
	old = MemoryContextSwitchTo(cur_bmbuild->tmpcxt);

	for(i = 0; i < numKeys; i++)
	{
	    if (isNull1[i] && isNull2[i])
        {
            /* both nulls -- treat as equal so we group them together */
        }
        else if ( isNull1[i] || isNull2[i])
        {
            /* one is null and one non-null -- this is inequal */
            result = 1;
            break;
        }
        else
        {
            /* do the real comparison */
            Datum attr1 = k1[i];
            Datum attr2 = k2[i];
            if (!DatumGetBool(FunctionCall2(&cur_bmbuild->eq_funcs[i], attr1, attr2)))
            {
                result = 1;     /* they aren't equal */
                break;
            }
        }
	}
	MemoryContextSwitchTo(old);
	return result;
}

static void *build_keycopy(void *dest, const void *src, Size keysize)
{
    BMBuildHashKey *destKey = (BMBuildHashKey*) dest;
    BMBuildHashKey *srcKey = (BMBuildHashKey*) src;
    int numKeys = cur_bmbuild->natts;

    Datum *datumsOut = (Datum*) (((char*)dest) + MAXALIGN(sizeof(BMBuildHashKey)));
    bool *isNullsOut = (bool*) (((char*)dest) + MAXALIGN(sizeof(BMBuildHashKey)) + MAXALIGN(sizeof(Datum) * numKeys ));
    int i;

    for ( i = 0; i < numKeys; i++)
    {
        datumsOut[i] = srcKey->attributeValueArr[i];
        isNullsOut[i] = srcKey->isNullArr[i];
    }

    /* we've copied the datums into the data segment, now set up final output */ 
    destKey->attributeValueArr = datumsOut;
    destKey->isNullArr  = isNullsOut;

    /**
     * build_keycopy must meet the spec of the keycopy function, which requires a return value even though
     * the return value is ignored 
     */
    return NULL;
}



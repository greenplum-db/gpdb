/*-------------------------------------------------------------------------
 *
 * bitmapattutil.c
 *	  Defines the routines to maintain all distinct attribute values
 *	  which are indexed in the on-disk bitmap index.
 *
 * Portions Copyright (c) 2007-2010 Greenplum Inc
 * Portions Copyright (c) 2010-2012 EMC Corporation
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 2006-2008, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/bitmap/bitmapattutil.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "access/tupdesc.h"
#include "access/bitmap.h"
#include "access/heapam.h"
#include "access/nbtree.h"
#include "access/xact.h"
#include "access/transam.h"
#include "catalog/dependency.h"
#include "catalog/gp_policy.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/pg_type.h"
#include "catalog/namespace.h"
#include "catalog/catalog.h"
#include "catalog/pg_namespace.h"
#include "commands/defrem.h"
#include "commands/tablecmds.h"
#include "nodes/execnodes.h"
#include "nodes/primnodes.h"
#include "nodes/makefuncs.h"
#include "optimizer/clauses.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/snapmgr.h"

static TupleDesc _bitmap_create_lov_heapTupleDesc(Relation rel);

/*
 * _bitmap_create_lov_heapandindex() -- create a new heap relation and
 *	a btree index for the list of values (LOV).
 *
 * Returns the OID of the created heap and btree index.
 */

void
_bitmap_create_lov_heapandindex(Relation rel,
								Oid *lovHeapOid,
								Oid *lovIndexOid)
{
	char		lovHeapName[NAMEDATALEN];
	char		lovIndexName[NAMEDATALEN];
	TupleDesc	tupDesc;
	IndexInfo  *indexInfo;
	ObjectAddress	objAddr, referenced;
	Oid		   *classObjectId;
	int16	   *coloptions;
	Oid			heapid;
	Oid			idxid;
	int			indattrs;
	int			i;

	Assert(rel != NULL);

	/* create the new names for the new lov heap and index */
	snprintf(lovHeapName, sizeof(lovHeapName),
			 "pg_bm_%u", RelationGetRelid(rel));
	snprintf(lovIndexName, sizeof(lovIndexName),
			 "pg_bm_%u_index", RelationGetRelid(rel));

	heapid = get_relname_relid(lovHeapName, PG_BITMAPINDEX_NAMESPACE);

	/*
	 * If heapid exists, then this is happening during re-indexing.
	 * We allocate new relfilenodes for lov heap and lov index.
	 *
	 * XXX Each segment db may have different relfilenodes for lov heap and
	 * lov index, which should not be an issue now. Ideally, we would like each
	 * segment db use the same oids.
	 */
	if (OidIsValid(heapid))
	{
		Relation lovHeap;
		Relation lovIndex;
		Buffer btree_metabuf;
		Page   btree_metapage;

		*lovHeapOid = heapid;

		idxid = get_relname_relid(lovIndexName, PG_BITMAPINDEX_NAMESPACE);
		Assert(OidIsValid(idxid));
		*lovIndexOid = idxid;

		lovHeap = heap_open(heapid, AccessExclusiveLock);
		lovIndex = index_open(idxid, AccessExclusiveLock);

		setNewRelfilenode(lovHeap, RecentXmin);
		setNewRelfilenode(lovIndex, RecentXmin);

		/*
		 * After creating the new relfilenode for a btee index, this is not
		 * a btree anymore. We create the new metapage for this btree.
		 */
		btree_metabuf = _bt_getbuf(lovIndex, P_NEW, BT_WRITE);
		Assert (BTREE_METAPAGE == BufferGetBlockNumber(btree_metabuf));
		btree_metapage = BufferGetPage(btree_metabuf);
		_bt_initmetapage(btree_metapage, P_NONE, 0);

		/* XLOG the metapage */

		if (!lovIndex->rd_istemp)
		{
			// Fetch gp_persistent_relation_node information that will be added to XLOG record.
			RelationFetchGpRelationNodeForXLog(lovIndex);

			log_newpage_rel(lovIndex, BufferGetBlockNumber(btree_metabuf),
						btree_metapage);
		}

		/* This cache value is not valid anymore. */
		if (lovIndex->rd_amcache)
		{
			pfree(lovIndex->rd_amcache);
			lovIndex->rd_amcache = NULL;
		}
		MarkBufferDirty(btree_metabuf);
		_bt_relbuf(lovIndex, btree_metabuf);

		index_close(lovIndex, NoLock);
		heap_close(lovHeap, NoLock);

		return;
	}

	/*
	 * create a new empty heap to store all attribute values with their
	 * corresponding block number and offset in LOV.
	 */
	tupDesc = _bitmap_create_lov_heapTupleDesc(rel);

	Assert(rel->rd_rel != NULL);

  	heapid =
		heap_create_with_catalog(lovHeapName, PG_BITMAPINDEX_NAMESPACE,
								 rel->rd_rel->reltablespace,
								 InvalidOid, rel->rd_rel->relowner,
								 tupDesc, NIL,
								 /* relam */ InvalidOid, RELKIND_RELATION, RELSTORAGE_HEAP,
								 rel->rd_rel->relisshared, false, /* bufferPoolBulkLoad */ false, 0,
								 ONCOMMIT_NOOP, NULL /* GP Policy */,
								 (Datum)0, true,
								 /* valid_opts */ true,
						 		 /* persistentTid */ NULL,
						 		 /* persistentSerialNum */ NULL);
	*lovHeapOid = heapid;

	/*
	 * We must bump the command counter to make the newly-created relation
	 * tuple visible for opening.
	 */
	CommandCounterIncrement();

	objAddr.classId = RelationRelationId;
	objAddr.objectId = heapid;
	objAddr.objectSubId = 0 ;

	referenced.classId = RelationRelationId;
	referenced.objectId = RelationGetRelid(rel);
	referenced.objectSubId = 0;

	recordDependencyOn(&objAddr, &referenced, DEPENDENCY_INTERNAL);

	/*
	 * create a btree index on the newly-created heap.
	 * The key includes all attributes to be indexed in this bitmap index.
	 */
	indattrs = tupDesc->natts - 2;
	indexInfo = makeNode(IndexInfo);
	indexInfo->ii_NumIndexAttrs = indattrs;
	indexInfo->ii_Expressions = NIL;
	indexInfo->ii_ExpressionsState = NIL;
	indexInfo->ii_Predicate = make_ands_implicit(NULL);
	indexInfo->ii_PredicateState = NIL;
	indexInfo->ii_Unique = true;

	classObjectId = (Oid *) palloc(indattrs * sizeof(Oid));
	coloptions = (int16 *) palloc(indattrs * sizeof(int16));
	for (i = 0; i < indattrs; i++)
	{
		Oid typid = tupDesc->attrs[i]->atttypid;

		indexInfo->ii_KeyAttrNumbers[i] = i + 1;
		classObjectId[i] = GetDefaultOpClass(typid, BTREE_AM_OID);
		coloptions[i] = 0;
	}

	idxid = index_create(heapid, lovIndexName, InvalidOid,
						 indexInfo, BTREE_AM_OID,
						 rel->rd_rel->reltablespace,
						 classObjectId, coloptions, 0, false, false, true,
						 false, false, NULL);
	*lovIndexOid = idxid;
}

/*
 * _bitmap_create_lov_heapTupleDesc() -- create the new heap tuple descriptor.
 */

TupleDesc
_bitmap_create_lov_heapTupleDesc(Relation rel)
{
	TupleDesc	tupDesc;
	TupleDesc	oldTupDesc;
	AttrNumber	attno;
	int			natts;

	oldTupDesc = RelationGetDescr(rel);
	natts = oldTupDesc->natts + 2;

	tupDesc = CreateTemplateTupleDesc(natts, false);

	for (attno = 1; attno <= oldTupDesc->natts; attno++)
	{
		/* copy the attribute to be indexed. */
		memcpy(tupDesc->attrs[attno - 1], oldTupDesc->attrs[attno - 1],
			   ATTRIBUTE_TUPLE_SIZE);
		tupDesc->attrs[attno - 1]->attnum = attno;
	}

	/* the block number */
	TupleDescInitEntry(tupDesc, attno, "blockNumber", INT4OID, -1, 0);
	attno++;

	/* the offset number */
	TupleDescInitEntry(tupDesc, attno, "offsetNumber", INT4OID, -1, 0);

	return tupDesc;
}

/*
 * _bitmap_open_lov_heapandindex() -- open the heap relation and the btree
 *		index for LOV.
 */

void
_bitmap_open_lov_heapandindex(Relation rel __attribute__((unused)), BMMetaPage metapage,
							  Relation *lovHeapP, Relation *lovIndexP,
							  LOCKMODE lockMode)
{
	*lovHeapP = heap_open(metapage->bm_lov_heapId, lockMode);
	*lovIndexP = index_open(metapage->bm_lov_indexId, lockMode);
}

/*
 * _bitmap_insert_lov() -- insert a new data into the given heap and index.
 */
void
_bitmap_insert_lov(Relation lovHeap, Relation lovIndex, Datum *datum,
				   bool *nulls, bool use_wal __attribute__((unused)))
{
	TupleDesc	tupDesc;
	HeapTuple	tuple;
	bool		result;
	Datum	   *indexDatum;
	bool	   *indexNulls;

	tupDesc = RelationGetDescr(lovHeap);

	/* insert this tuple into the heap */
	tuple = heap_form_tuple(tupDesc, datum, nulls);
	frozen_heap_insert(lovHeap, tuple);

	/* insert a new tuple into the index */
	indexDatum = palloc0((tupDesc->natts - 2) * sizeof(Datum));
	indexNulls = palloc0((tupDesc->natts - 2) * sizeof(bool));
	memcpy(indexDatum, datum, (tupDesc->natts - 2) * sizeof(Datum));
	memcpy(indexNulls, nulls, (tupDesc->natts - 2) * sizeof(bool));
	result = index_insert(lovIndex, indexDatum, indexNulls,
					 	  &(tuple->t_self), lovHeap, true);

	pfree(indexDatum);
	pfree(indexNulls);
	Assert(result);

	heap_freetuple(tuple);
}


/*
 * _bitmap_close_lov_heapandindex() -- close the heap and the index.
 */
void
_bitmap_close_lov_heapandindex(Relation lovHeap, Relation lovIndex,
							   LOCKMODE lockMode)
{
	heap_close(lovHeap, lockMode);
	index_close(lovIndex, lockMode);
}

/*
 * _bitmap_findvalue() -- find a row in a given heap using
 *  a given index that satisfies the given scan key.
 *
 * If this value exists, this function returns true. Otherwise,
 * returns false.
 *
 * If this value exists in the heap, this function also returns
 * the block number and the offset number that are stored in the same
 * row with this value. This block number and the offset number
 * are for the LOV item that points the bitmap vector for this value.
 */
bool
_bitmap_findvalue(Relation lovHeap, Relation lovIndex,
				  ScanKey scanKey __attribute__((unused)), IndexScanDesc scanDesc,
				  BlockNumber *lovBlock, bool *blockNull,
				  OffsetNumber *lovOffset, bool *offsetNull)
{
	TupleDesc		tupDesc;
	HeapTuple		tuple;
	bool			found = false;

	tupDesc = RelationGetDescr(lovIndex);

	tuple = index_getnext(scanDesc, ForwardScanDirection);

	if (tuple != NULL)
	{
		TupleDesc 	heapTupDesc;
		Datum 		d;

		found = true;
		heapTupDesc = RelationGetDescr(lovHeap);

		d = heap_getattr(tuple, tupDesc->natts + 1, heapTupDesc, blockNull);
		*lovBlock =	DatumGetInt32(d);
		d = heap_getattr(tuple, tupDesc->natts + 2, heapTupDesc, offsetNull);
		*lovOffset = DatumGetInt16(d);
	}
	return found;
}


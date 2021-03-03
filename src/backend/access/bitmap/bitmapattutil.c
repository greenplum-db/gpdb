/*-------------------------------------------------------------------------
 *
 * bitmapattutil.c
 *	  Defines the routines to maintain all distinct attribute values
 *	  which are indexed in the on-disk bitmap index.
 *
 * Portions Copyright (c) 2007-2010 Greenplum Inc
 * Portions Copyright (c) 2010-2012 EMC Corporation
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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
#include "access/bitmap_private.h"
#include "access/heapam.h"
#include "access/heapam_xlog.h"
#include "access/multixact.h"
#include "access/nbtree.h"
#include "access/xact.h"
#include "access/transam.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/pg_am.h"
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
	List	   *indexColNames;
	ObjectAddress	objAddr, referenced;
	Oid		   *classObjectId;
	int16	   *coloptions;
	Oid		   *colcollations;
	Oid			heapid;
	Oid			idxid;
	int			indattrs;
	int			i;
	Relation	lov_heap_rel;
	Oid			namespaceid;

	Assert(rel != NULL);

	/* create the new names for the new lov heap and index */
	snprintf(lovHeapName, sizeof(lovHeapName),
			 "pg_bm_%u", RelationGetRelid(rel));
	snprintf(lovIndexName, sizeof(lovIndexName),
			 "pg_bm_%u_index", RelationGetRelid(rel));

	/*
	 * The LOV heap and index go in special pg_bitmapindex schema. Those for
	 * temp relations go into the per-backend temp-toast-table namespace,
	 * however!
	 */
	if (RelationUsesTempNamespace(rel))
		namespaceid = GetTempToastNamespace();
	else
		namespaceid = PG_BITMAPINDEX_NAMESPACE;

	heapid = get_relname_relid(lovHeapName, namespaceid);

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

		idxid = get_relname_relid(lovIndexName, namespaceid);
		Assert(OidIsValid(idxid));
		*lovIndexOid = idxid;

		lovHeap = heap_open(heapid, AccessExclusiveLock);
		lovIndex = index_open(idxid, AccessExclusiveLock);

		RelationSetNewRelfilenode(lovHeap, lovHeap->rd_rel->relpersistence);
		RelationSetNewRelfilenode(lovIndex, lovIndex->rd_rel->relpersistence);

		/*
		 * After creating the new relfilenode for a btee index, this is not
		 * a btree anymore. We create the new metapage for this btree.
		 */
		btree_metabuf = _bt_getbuf(lovIndex, P_NEW, BT_WRITE);
		Assert (BTREE_METAPAGE == BufferGetBlockNumber(btree_metabuf));
		btree_metapage = BufferGetPage(btree_metabuf);
		_bt_initmetapage(btree_metapage, P_NONE, 0);

		/* XLOG the metapage */

		if (RelationNeedsWAL(lovIndex))
		{
			START_CRIT_SECTION();
			MarkBufferDirty(btree_metabuf);
			log_newpage_buffer(btree_metabuf, true);
			END_CRIT_SECTION();
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
		heap_create_with_catalog(lovHeapName,
								 namespaceid,
								 rel->rd_rel->reltablespace,
								 InvalidOid,
								 InvalidOid,
								 InvalidOid,
								 rel->rd_rel->relowner,
								 HEAP_TABLE_AM_OID,
								 tupDesc, NIL,
								 RELKIND_RELATION,
								 rel->rd_rel->relpersistence,
								 rel->rd_rel->relisshared,
								 false, /* mapped_relation */
								 ONCOMMIT_NOOP, NULL /* GP Policy */,
								 (Datum)0, false, true,
								 true, /* is_internal */
								 InvalidOid, /* relrewrite */
								 NULL, /* typeaddress */
								 /* valid_opts */ true);
	*lovHeapOid = heapid;

	/*
	 * We must bump the command counter to make the newly-created relation
	 * tuple visible for opening.
	 */
	CommandCounterIncrement();

	/* ShareLock is not really needed here, but take it anyway */
	lov_heap_rel = heap_open(heapid, ShareLock);

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
	indexInfo->ii_NumIndexKeyAttrs = indattrs;
	indexInfo->ii_Expressions = NIL;
	indexInfo->ii_ExpressionsState = NIL;
	indexInfo->ii_Predicate = NIL;
	indexInfo->ii_PredicateState = NULL;
	indexInfo->ii_ExclusionOps = NULL;
	indexInfo->ii_ExclusionProcs = NULL;
	indexInfo->ii_ExclusionStrats = NULL;
	indexInfo->ii_Unique = true;
	indexInfo->ii_ReadyForInserts = true;
	indexInfo->ii_Concurrent = false;
	indexInfo->ii_BrokenHotChain = false;
	indexInfo->ii_ParallelWorkers = 0;
	indexInfo->ii_Am = BTREE_AM_OID;
	indexInfo->ii_AmCache = NULL;
	indexInfo->ii_Context = CurrentMemoryContext;

	classObjectId = (Oid *) palloc(indattrs * sizeof(Oid));
	coloptions = (int16 *) palloc(indattrs * sizeof(int16));
	colcollations = (Oid *) palloc(indattrs * sizeof(Oid));
	indexColNames = NIL;
	for (i = 0; i < indattrs; i++)
	{
		Form_pg_attribute attr = TupleDescAttr(tupDesc, i);

		indexInfo->ii_IndexAttrNumbers[i] = i + 1;
		classObjectId[i] = GetDefaultOpClass(attr->atttypid, BTREE_AM_OID);
		coloptions[i] = 0;
		colcollations[i] = rel->rd_indcollation[i];

		indexColNames = lappend(indexColNames, NameStr(attr->attname));
	}

	idxid = index_create(lov_heap_rel, lovIndexName, InvalidOid,
						 InvalidOid,
						 InvalidOid,
						 InvalidOid,
						 indexInfo,
						 indexColNames,
						 BTREE_AM_OID,
						 rel->rd_rel->reltablespace,
						 colcollations,
						 classObjectId, coloptions, (Datum) 0,
						 INDEX_CREATE_IF_NOT_EXISTS, /* flags */
						 0, /* constr_flags */
						 /* allow_system_table_mods */ true,
						 /* is_internal */ true,
						 NULL);
	*lovIndexOid = idxid;

	heap_close(lov_heap_rel, NoLock);
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

	tupDesc = CreateTemplateTupleDesc(natts);

	for (attno = 1; attno <= oldTupDesc->natts; attno++)
	{
		/* copy the attribute to be indexed. */
		TupleDescCopyEntry(tupDesc, attno, oldTupDesc, attno);
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
_bitmap_open_lov_heapandindex(Relation rel pg_attribute_unused(), BMMetaPage metapage,
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
				   bool *nulls, bool use_wal pg_attribute_unused())
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
					 	  &(tuple->t_self), lovHeap, true, NULL);

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
				  ScanKey scanKey pg_attribute_unused(), IndexScanDesc scanDesc,
				  BlockNumber *lovBlock, bool *blockNull,
				  OffsetNumber *lovOffset, bool *offsetNull)
{
	TupleDesc		tupDesc;
	bool			found = false;
	TupleTableSlot *slot;

	tupDesc = RelationGetDescr(lovIndex);

	/*
	 * creating a new slot on every call is a bit expensive, but there's no
	 * convenient place to keep it.
	 */
	slot = table_slot_create(lovHeap, NULL);
	if (index_getnext_slot(scanDesc, ForwardScanDirection, slot))
	{
		Datum 		d;

		found = true;

		d = slot_getattr(slot, tupDesc->natts + 1, blockNull);
		*lovBlock =	DatumGetInt32(d);
		d = slot_getattr(slot, tupDesc->natts + 2, offsetNull);
		*lovOffset = DatumGetInt16(d);
	}

	ExecDropSingleTupleTableSlot(slot);

	return found;
}


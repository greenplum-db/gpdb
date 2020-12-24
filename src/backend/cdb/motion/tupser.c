/*-------------------------------------------------------------------------
 * tupser.c
 *	   Functions for serializing and deserializing a heap tuple.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/motion/tupser.c
 *
 *      Reviewers: jzhang, ftian, tkordas
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup.h"
#include "access/memtup.h"
#include "access/tuptoaster.h"
#include "catalog/pg_type.h"
#include "cdb/cdbmotion.h"
#include "cdb/cdbsrlz.h"
#include "cdb/tupser.h"
#include "cdb/cdbvars.h"
#include "libpq/pqformat.h"
#include "storage/smgr.h"
#include "utils/acl.h"
#include "utils/date.h"
#include "utils/numeric.h"
#include "utils/memutils.h"
#include "utils/builtins.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

/*
 * Transient record types table is sent to upsteam via a specially constructed
 * chunk, with a special "tuple length".
 */
#define RECORD_CACHE_MAGIC_TUPLEN	-1

/* A MemoryContext used within the tuple serialize code, so that freeing of
 * space is SUPAFAST.  It is initialized in the first call to InitSerTupInfo()
 * since that must be called before any tuple serialization or deserialization
 * work can be done.
 */
static MemoryContext s_tupSerMemCtxt = NULL;

static void addByteStringToChunkList(TupleChunkList tcList, char *data, int datalen, TupleChunkListCache *cache);

#define addCharToChunkList(tcList, x, c)							\
	do															\
	{															\
		char y = (x);											\
		addByteStringToChunkList((tcList), (char *)&y, sizeof(y), (c));	\
	} while (0)

#define addInt32ToChunkList(tcList, x, c)							\
	do															\
	{															\
		int32 y = (x);											\
		addByteStringToChunkList((tcList), (char *)&y, sizeof(y), (c));	\
	} while (0)


/* Look up all of the information that SerializeTuple() and DeserializeTuple()
 * need to perform their jobs quickly.	Also, scratchpad space is allocated
 * for serialization and desrialization of datum values, and for formation/
 * deformation of tuples themselves.
 *
 * NOTE:  This function allocates various data-structures, but it assumes that
 *		  the current memory-context is acceptable.  So the caller should set
 *		  the desired memory-context before calling this function.
 */
void
InitSerTupInfo(TupleDesc tupdesc, SerTupInfo *pSerInfo)
{
	int			i,
				numAttrs;

	AssertArg(tupdesc != NULL);
	AssertArg(pSerInfo != NULL);

	if (s_tupSerMemCtxt == NULL)
	{
		/* Create tuple-serialization memory context. */
		s_tupSerMemCtxt =
			AllocSetContextCreate(TopMemoryContext,
								  "TupSerMemCtxt",
								  ALLOCSET_DEFAULT_INITSIZE,	/* always have some
																 * memory */
								  ALLOCSET_DEFAULT_INITSIZE,
								  ALLOCSET_DEFAULT_MAXSIZE);
	}

	/* Set contents to all 0, just to make things clean and easy. */
	memset(pSerInfo, 0, sizeof(SerTupInfo));

	/* Store the tuple-descriptor so we can use it later. */
	pSerInfo->tupdesc = tupdesc;

	pSerInfo->chunkCache.len = 0;
	pSerInfo->chunkCache.items = NULL;

	pSerInfo->has_record_types = false;

	/*
	 * If we have some attributes, go ahead and prepare the information for
	 * each attribute in the descriptor.  Otherwise, we can return right away.
	 */
	numAttrs = tupdesc->natts;
	if (numAttrs <= 0)
		return;

	pSerInfo->myinfo = (SerAttrInfo *) palloc0(numAttrs * sizeof(SerAttrInfo));

	pSerInfo->values = (Datum *) palloc(numAttrs * sizeof(Datum));
	pSerInfo->nulls = (bool *) palloc(numAttrs * sizeof(bool));

	for (i = 0; i < numAttrs; i++)
	{
		SerAttrInfo *attrInfo = pSerInfo->myinfo + i;

		/*
		 * Get attribute's data-type Oid.  This lets us shortcut the comm
		 * operations for some attribute-types.
		 */
		attrInfo->atttypid = TupleDescAttr(tupdesc, i)->atttypid;

		/*
		 * Serialization will be performed at a high level abstraction, we
		 * only care about whether it's toasted or pass-by-value or a CString,
		 * so only track the high level type information.
		 */
		{
			HeapTuple	typeTuple;
			Form_pg_type pt;

			typeTuple = SearchSysCache1(TYPEOID,
										ObjectIdGetDatum(attrInfo->atttypid));
			if (!HeapTupleIsValid(typeTuple))
				elog(ERROR, "cache lookup failed for type %u", attrInfo->atttypid);
			pt = (Form_pg_type) GETSTRUCT(typeTuple);

			/*
			 * Consider any non-basic types as potential containers of record
			 * types
			 */
			if (pt->typtype != TYPTYPE_BASE)
				pSerInfo->has_record_types = true;

			if (!pt->typisdefined)
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
						 errmsg("type %s is only a shell",
								format_type_be(attrInfo->atttypid))));

			attrInfo->typlen = pt->typlen;
			attrInfo->typbyval = pt->typbyval;

			ReleaseSysCache(typeTuple);
		}
	}
}


/* Free up storage in a previously initialized SerTupInfo struct. */
void
CleanupSerTupInfo(SerTupInfo *pSerInfo)
{
	AssertArg(pSerInfo != NULL);

	/*
	 * Free any old data.
	 *
	 * NOTE:  This works because data-structure was bzero()ed in init call.
	 */
	if (pSerInfo->myinfo != NULL)
		pfree(pSerInfo->myinfo);
	pSerInfo->myinfo = NULL;

	if (pSerInfo->values != NULL)
		pfree(pSerInfo->values);
	pSerInfo->values = NULL;

	if (pSerInfo->nulls != NULL)
		pfree(pSerInfo->nulls);
	pSerInfo->nulls = NULL;

	pSerInfo->tupdesc = NULL;

	while (pSerInfo->chunkCache.items != NULL)
	{
		TupleChunkListItem item;

		item = pSerInfo->chunkCache.items;
		pSerInfo->chunkCache.items = item->p_next;
		pfree(item);
	}
}

/*
 * When manipulating chunks before transmit, it is important to notice that the
 * tcItem's chunk_length field *includes* the 4-byte chunk header, but that the
 * length within the header itself does not. Getting the two confused results
 * heap overruns and that way lies pain.
 */
static void
addByteStringToChunkList(TupleChunkList tcList, char *data, int datalen, TupleChunkListCache *chunkCache)
{
	TupleChunkListItem tcItem;
	int			remain,
				curSize,
				available,
				copyLen;
	char	   *pos;

	AssertArg(tcList != NULL);
	AssertArg(tcList->p_last != NULL);
	AssertArg(data != NULL);

	/* Add onto last chunk, lists always start with one chunk */
	tcItem = tcList->p_last;

	/* we'll need to add chunks */
	remain = datalen;
	pos = data;
	do
	{
		curSize = tcItem->chunk_length;

		available = tcList->max_chunk_length - curSize;
		copyLen = Min(available, remain);
		if (copyLen > 0)
		{
			/*
			 * make sure we don't stomp on the serialized header, chunk_length
			 * already accounts for it
			 */
			memcpy(tcItem->chunk_data + curSize, pos, copyLen);

			remain -= copyLen;
			pos += copyLen;
			tcList->serialized_data_length += copyLen;
			curSize += copyLen;

			SetChunkDataSize(tcItem->chunk_data, curSize - TUPLE_CHUNK_HEADER_SIZE);
			tcItem->chunk_length = curSize;
		}

		if (remain == 0)
			break;

		tcItem = getChunkFromCache(chunkCache);
		tcItem->chunk_length = TUPLE_CHUNK_HEADER_SIZE;
		SetChunkType(tcItem->chunk_data, TC_PARTIAL_MID);
		appendChunkToTCList(tcList, tcItem);
	} while (remain != 0);

	return;
}

/*
 * Convert RecordCache into a byte-sequence, and store it directly
 * into a chunklist for transmission.
 *
 * This code is based on the printtup_internal_20() function in printtup.c.
 */
void
SerializeRecordCacheIntoChunks(SerTupInfo *pSerInfo,
							   TupleChunkList tcList,
							   MotionConn *conn)
{
	TupleChunkListItem tcItem = NULL;
	MemoryContext oldCtxt;
	List	   *typelist = NULL;
	int			size = -1;
	char	   *buf = NULL;

	AssertArg(tcList != NULL);
	AssertArg(pSerInfo != NULL);

	/* get ready to go */
	tcList->p_first = NULL;
	tcList->p_last = NULL;
	tcList->num_chunks = 0;
	tcList->serialized_data_length = 0;
	tcList->max_chunk_length = Gp_max_tuple_chunk_size;

	tcItem = getChunkFromCache(&pSerInfo->chunkCache);

	/* assume that we'll take a single chunk */
	SetChunkType(tcItem->chunk_data, TC_WHOLE);
	tcItem->chunk_length = TUPLE_CHUNK_HEADER_SIZE;
	appendChunkToTCList(tcList, tcItem);

	AssertState(s_tupSerMemCtxt != NULL);

	/*
	 * To avoid inconsistency of record cache between sender and receiver in
	 * the same motion, send the serialized record cache to receiver before
	 * the first tuple is sent, the receiver is responsible for registering
	 * the records to its own local cache and remapping the typmod of tuples
	 * sent by sender.
	 */
	oldCtxt = MemoryContextSwitchTo(s_tupSerMemCtxt);
	typelist = build_tuple_node_list(conn->sent_record_typmod);
	buf = serializeNode((Node *) typelist, &size, NULL);
	MemoryContextSwitchTo(oldCtxt);

	/*
	 * we use magic tuplen to identify that this chunk (or list of chunks)
	 * actually carries the serialized record cache table.
	 */
	int tupbodylen = RECORD_CACHE_MAGIC_TUPLEN;

	addByteStringToChunkList(tcList, (char *) &tupbodylen, sizeof(int),
							 &pSerInfo->chunkCache);
	/* Now write out the real length */
	addByteStringToChunkList(tcList, (char *) &size, sizeof(int),
							 &pSerInfo->chunkCache);
	addByteStringToChunkList(tcList, buf, size, &pSerInfo->chunkCache);

	/*
	 * if we have more than 1 chunk we have to set the chunk types on our
	 * first chunk and last chunk
	 */
	if (tcList->num_chunks > 1)
	{
		TupleChunkListItem first,
					last;

		first = tcList->p_first;
		last = tcList->p_last;

		Assert(first != NULL);
		Assert(first != last);
		Assert(last != NULL);

		SetChunkType(first->chunk_data, TC_PARTIAL_START);
		SetChunkType(last->chunk_data, TC_PARTIAL_END);

		/*
		 * any intervening chunks are already set to TC_PARTIAL_MID when
		 * allocated
		 */
	}

	return;
}

static bool
CandidateForSerializeDirect(int16 targetRoute, struct directTransportBuffer *b)
{
	return targetRoute != BROADCAST_SEGIDX && b->pri != NULL && b->prilen > TUPLE_CHUNK_HEADER_SIZE;
}

/*
 *
 * First try to serialize a tuple directly into a buffer.
 *
 * We're called with at least enough space for a tuple-chunk-header.
 *
 * Convert a HeapTuple into a byte-sequence, and store it directly
 * into a chunklist for transmission.
 *
 * This code is based on the printtup_internal_20() function in printtup.c.
 */
int
SerializeTuple(TupleTableSlot *slot, SerTupInfo *pSerInfo, struct directTransportBuffer *b, TupleChunkList tcList, int16 targetRoute)
{
	int                natts;
	int                dataSize = TUPLE_CHUNK_HEADER_SIZE;
	TupleDesc          tupdesc;
	TupleChunkListItem tcItem = NULL;
	MinimalTuple       mintuple;
	bool               shouldFreeTuple;
	char               *tupbody;
	unsigned int       tupbodylen;
	unsigned int       tuplen;
	bool               hasExternalAttr = false;

	AssertArg(pSerInfo != NULL);
	AssertArg(b != NULL);

	tupdesc = pSerInfo->tupdesc;
	natts = tupdesc->natts;

	if (natts == 0 && CandidateForSerializeDirect(targetRoute, b))
	{
		/* TC_EMPTY is just one chunk */
		SetChunkType(b->pri, TC_EMPTY);
		SetChunkDataSize(b->pri, 0);

		return TUPLE_CHUNK_HEADER_SIZE;
	}

	tcList->p_first = NULL;
	tcList->p_last = NULL;
	tcList->num_chunks = 0;
	tcList->serialized_data_length = 0;
	tcList->max_chunk_length = Gp_max_tuple_chunk_size;

	/*
	 * GPDB_12_MERGE_FIXME: This used to support serializing memtuples directly.
	 * That got removed with MinimalTuples in the merge. Resurrect the MemtUple
	 * support if there's a performance benefit.
	 */
	/* Check if the slot has external attribute */
	for (int i = 0; i < natts; i++)
	{
		Form_pg_attribute attr = TupleDescAttr(slot->tts_tupleDescriptor, i);

		if (!attr->attisdropped && attr->attlen == -1 && !slot->tts_isnull[i])
		{
			hasExternalAttr = true;
			break;
		}
	}

	/*
	 * If the slot contains any toasted attributes, detoast them now before serializing
	 */
	if (hasExternalAttr)
	{
		Datum	   *values;

		slot_getallattrs(slot);
		values = slot->tts_values;

		for (int i = 0; i < natts; i++)
		{
			Datum		val = slot->tts_values[i];
			Form_pg_attribute attr = TupleDescAttr(slot->tts_tupleDescriptor, i);

			if (!attr->attisdropped && attr->attlen == -1 && !slot->tts_isnull[i])
			{
				if (VARATT_IS_EXTERNAL(DatumGetPointer(val)))
				{
					if (values == slot->tts_values)
					{
						values = (Datum *) palloc(tupdesc->natts * sizeof(Datum));
						memcpy(values, slot->tts_values, tupdesc->natts * sizeof(Datum));
					}
					Assert(&values[i] != &slot->tts_values[i]);
					values[i] = PointerGetDatum(heap_tuple_fetch_attr((struct varlena *)
																DatumGetPointer(val)));
				}
			}
		}
		mintuple = heap_form_minimal_tuple(slot->tts_tupleDescriptor, values,
										   slot->tts_isnull);
		if (values != slot->tts_values)
			pfree(values);

		shouldFreeTuple = true;
	}
	else
		mintuple = ExecFetchSlotMinimalTuple(slot, &shouldFreeTuple);

	tupbody = (char *) mintuple + MINIMAL_TUPLE_DATA_OFFSET;
	tupbodylen = mintuple->t_len - MINIMAL_TUPLE_DATA_OFFSET;

	/* total on-wire footprint: */
	tuplen = tupbodylen + sizeof(int);

	if (CandidateForSerializeDirect(targetRoute, b) &&
		tuplen + TUPLE_CHUNK_HEADER_SIZE <= b->prilen)
	{
		/*
		 * The tuple fits in the direct transport buffer.
		 */
		memcpy(b->pri + TUPLE_CHUNK_HEADER_SIZE, &tupbodylen, sizeof(tupbodylen));
		memcpy(b->pri + TUPLE_CHUNK_HEADER_SIZE + sizeof(int), tupbody, tupbodylen);

		dataSize += tuplen;

		SetChunkType(b->pri, TC_WHOLE);
		SetChunkDataSize(b->pri, dataSize - TUPLE_CHUNK_HEADER_SIZE);

		if (shouldFreeTuple)
			pfree(mintuple);
		return dataSize;
	}

	/*
	 * If direct in-line serialization failed then we fallback to chunked
	 * out-of-line serialization.
	 */
	tcItem = getChunkFromCache(&pSerInfo->chunkCache);
	SetChunkType(tcItem->chunk_data, TC_WHOLE);
	tcItem->chunk_length = TUPLE_CHUNK_HEADER_SIZE;
	appendChunkToTCList(tcList, tcItem);

	AssertState(s_tupSerMemCtxt != NULL);

	addByteStringToChunkList(tcList, (char *) &tupbodylen, sizeof(tupbodylen), &pSerInfo->chunkCache);
	addByteStringToChunkList(tcList, tupbody, tupbodylen, &pSerInfo->chunkCache);

	/*
	 * GPDB_12_MERGE_FIXME: This function does not use this context. This context
	 * is only used in SerializeRecordCacheIntoChunks(). We need to find a better
	 * place for resetting it, or eliminating the needs for this context.
	 */
	MemoryContextReset(s_tupSerMemCtxt);

	/*
	 * if we have more than 1 chunk we have to set the chunk types on our
	 * first chunk and last chunk
	 */
	if (tcList->num_chunks > 1)
	{
		TupleChunkListItem first,
					last;

		first = tcList->p_first;
		last = tcList->p_last;

		Assert(first != NULL);
		Assert(first != last);
		Assert(last != NULL);

		SetChunkType(first->chunk_data, TC_PARTIAL_START);
		SetChunkType(last->chunk_data, TC_PARTIAL_END);

		/*
		 * any intervening chunks are already set to TC_PARTIAL_MID when
		 * allocated
		 */
	}

	if (shouldFreeTuple)
		pfree(mintuple);

	/*
	 * performed "out-of-line" serialization
	 */
	return 0;
}

/*
 * Reassemble and deserialize a list of tuple chunks, into a tuple.
 */
MinimalTuple
CvtChunksToTup(TupleChunkList tcList, SerTupInfo *pSerInfo, TupleRemapper *remapper)
{
	StringInfoData serData;
	bool		serDataMustFree;
	TupleChunkListItem tcItem;
	TupleChunkListItem firstTcItem;
	MinimalTuple tup;
	TupleChunkType tcType;

	AssertArg(tcList != NULL);
	AssertArg(tcList->p_first != NULL);
	AssertArg(pSerInfo != NULL);

	/*
	 * Parse the first chunk, and reassemble the chunks if needed.
	 */
	firstTcItem = tcList->p_first;

	GetChunkType(firstTcItem, &tcType);

	if (tcType == TC_WHOLE)
	{
		if (firstTcItem->p_next)
			ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION),
							errmsg("Single chunk's type must be TC_WHOLE.")));

		/*
		 * We cheat a little, and point the StringInfo's buffer directly to the
		 * incoming data. This saves a palloc and memcpy.
		 *
		 * NB: We mustn't modify the string buffer!
		 */
		serData.data = (char *) GetChunkDataPtr(firstTcItem) + TUPLE_CHUNK_HEADER_SIZE;
		serData.len = serData.maxlen = firstTcItem->chunk_length - TUPLE_CHUNK_HEADER_SIZE;
		serData.cursor = 0;
		serDataMustFree = false;
	}
	else if (tcType == TC_EMPTY)
	{
		/*
		 * the sender is indicating that there was a row with no
		 * attributes: return a NULL tuple
		 */
		return heap_form_minimal_tuple(pSerInfo->tupdesc, pSerInfo->values, pSerInfo->nulls);
	}
	else if (tcType == TC_PARTIAL_START)
	{
		/*
		 * Re-assemble the chunks into a contiguous buffer..
		 */
		int			total_len;
		char	   *pos;

		/* Sanity-check the chunk types, and compute total length. */
		total_len = firstTcItem->chunk_length - TUPLE_CHUNK_HEADER_SIZE;

		tcItem = firstTcItem->p_next;
		while (tcItem != NULL)
		{
			int			this_len = tcItem->chunk_length - TUPLE_CHUNK_HEADER_SIZE;

			GetChunkType(tcItem, &tcType);

			if (tcItem->p_next == NULL)
			{
				if (tcType != TC_PARTIAL_END)
					ereport(ERROR,
							(errcode(ERRCODE_PROTOCOL_VIOLATION),
							 errmsg("last chunk of collection must have type TC_PARTIAL_END")));
			}
			else
			{
				if (tcType != TC_PARTIAL_MID)
					ereport(ERROR,
							(errcode(ERRCODE_PROTOCOL_VIOLATION),
							 errmsg("middle chunk of collection must have type TC_PARTIAL_MID")));
			}
			total_len += this_len;

			/* make sure we don't overflow total_len */
			if (this_len > MaxAllocSize || total_len > MaxAllocSize)
				ereport(ERROR,
						(errcode(ERRCODE_PROTOCOL_VIOLATION),
						 errmsg("chunked tuple is too large")));

			/* go to the next chunk. */
			tcItem = tcItem->p_next;
		}

		serData.data = palloc(total_len);
		serData.len = serData.maxlen = total_len;
		serData.cursor = 0;
		serDataMustFree = true;

		/* Copy the data from each chunk into the buffer.  Don't include the headers! */
		pos = serData.data;
		tcItem = firstTcItem;
		while (tcItem != NULL)
		{
			int			this_len = tcItem->chunk_length - TUPLE_CHUNK_HEADER_SIZE;

			memcpy(pos,
				   (const char *) GetChunkDataPtr(tcItem) + TUPLE_CHUNK_HEADER_SIZE,
				   this_len);
			pos += this_len;

			tcItem = tcItem->p_next;
		}
	}
	else
	{
		/*
		 * The caller handles TC_END_OF_STREAM directly, so we should not see
		 * them here.
		 *
		 * TC_PARTIAL_MID/END should not appear at the beginning of a chunk
		 * list, without TC_PARTIAL_START.
		 */
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("unexpected tuple chunk type %d at beginning of chunk list", tcType)));
	}

	/* We now have the reassembled data in 'serData'. Deserialize it back to a tuple. */
	{
		char	   *pos = serData.data;
		int			tupbodylen;

		/* read length */
		memcpy(&tupbodylen, pos, sizeof(tupbodylen));
		pos += sizeof(tupbodylen);

		if (tupbodylen == RECORD_CACHE_MAGIC_TUPLEN)
		{
			/* a special tuple with record type cache */
			int			size;
			List	   *typelist;

			memcpy(&size, pos, sizeof(size));
			pos += sizeof(size);

			typelist = (List *) deserializeNode(pos, size);
			pos += size;

			TRHandleTypeLists(remapper, typelist);

			/* Free up memory we used. */
			if (serDataMustFree)
				pfree(serData.data);

			return NULL;
		}
		else
		{
			/* A normal MinimalTuple */
			unsigned int tuplen = tupbodylen + MINIMAL_TUPLE_DATA_OFFSET;
			char	   *tupbody;

			tup = palloc(tuplen);
			tup->t_len = tuplen;

			tupbody = (char *) tup + MINIMAL_TUPLE_DATA_OFFSET;
			memcpy(tupbody, pos, tupbodylen);
		}
	}

	/* Free up memory we used. */
	if (serDataMustFree)
		pfree(serData.data);

	return tup;
}

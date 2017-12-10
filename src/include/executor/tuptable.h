/*-------------------------------------------------------------------------
 *
 * tuptable.h
 *	  tuple table support stuff
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/executor/tuptable.h,v 1.42 2009/06/11 14:49:11 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef TUPTABLE_H
#define TUPTABLE_H

#include "access/htup.h"
#include "access/tupdesc.h"
#include "access/heapam.h"
#include "access/memtup.h"
#include "storage/buf.h"
#include "codegen/codegen_wrapper.h"

/*----------
 * The executor stores tuples in a "tuple table" which is a List of
 * independent TupleTableSlots.  There are several cases we need to handle:
 *		1. physical tuple in a disk buffer page
 *		2. physical tuple constructed in palloc'ed memory
 *		3. "minimal" physical tuple constructed in palloc'ed memory
 *		4. "virtual" tuple consisting of Datum/isnull arrays
 *
 * The first two cases are similar in that they both deal with "materialized"
 * tuples, but resource management is different.  For a tuple in a disk page
 * we need to hold a pin on the buffer until the TupleTableSlot's reference
 * to the tuple is dropped; while for a palloc'd tuple we usually want the
 * tuple pfree'd when the TupleTableSlot's reference is dropped.
 *
 * A "minimal" tuple is handled similarly to a palloc'd regular tuple.
 * At present, minimal tuples never are stored in buffers, so there is no
 * parallel to case 1.	Note that a minimal tuple has no "system columns".
 * (Actually, it could have an OID, but we have no need to access the OID.)
 *
 * A "virtual" tuple is an optimization used to minimize physical data
 * copying in a nest of plan nodes.  Any pass-by-reference Datums in the
 * tuple point to storage that is not directly associated with the
 * TupleTableSlot; generally they will point to part of a tuple stored in
 * a lower plan node's output TupleTableSlot, or to a function result
 * constructed in a plan node's per-tuple econtext.  It is the responsibility
 * of the generating plan node to be sure these resources are not released
 * for as long as the virtual tuple needs to be valid.	We only use virtual
 * tuples in the result slots of plan nodes --- tuples to be copied anywhere
 * else need to be "materialized" into physical tuples.  Note also that a
 * virtual tuple does not have any "system columns".
 *
 * It is also possible for a TupleTableSlot to hold both physical and minimal
 * copies of a tuple.  This is done when the slot is requested to provide
 * the format other than the one it currently holds.  (Originally we attempted
 * to handle such requests by replacing one format with the other, but that
 * had the fatal defect of invalidating any pass-by-reference Datums pointing
 * into the existing slot contents.)  Both copies must contain identical data
 * payloads when this is the case.
 *
 * The Datum/isnull arrays of a TupleTableSlot serve double duty.  When the
 * slot contains a virtual tuple, they are the authoritative data.	When the
 * slot contains a physical tuple, the arrays contain data extracted from
 * the tuple.  (In this state, any pass-by-reference Datums point into
 * the physical tuple.)  The extracted information is built "lazily",
 * ie, only as needed.	This serves to avoid repeated extraction of data
 * from the physical tuple.
 *
 * A TupleTableSlot can also be "empty", holding no valid data.  This is
 * the only valid state for a freshly-created slot that has not yet had a
 * tuple descriptor assigned to it.  In this state, tts_isempty must be
 * TRUE, tts_shouldFree FALSE, tts_tuple NULL, tts_buffer InvalidBuffer,
 * and tts_nvalid zero.
 *
 * The tupleDescriptor is simply referenced, not copied, by the TupleTableSlot
 * code.  The caller of ExecSetSlotDescriptor() is responsible for providing
 * a descriptor that will live as long as the slot does.  (Typically, both
 * slots and descriptors are in per-query memory and are freed by memory
 * context deallocation at query end; so it's not worth providing any extra
 * mechanism to do more.  However, the slot will increment the tupdesc
 * reference count if a reference-counted tupdesc is supplied.)
 *
 * When tts_shouldFree is true, the physical tuple is "owned" by the slot
 * and should be freed when the slot's reference to the tuple is dropped.
 *
 * If tts_buffer is not InvalidBuffer, then the slot is holding a pin
 * on the indicated buffer page; drop the pin when we release the
 * slot's reference to that buffer.  (tts_shouldFree should always be
 * false in such a case, since presumably tts_tuple is pointing at the
 * buffer page.)
 *
 * tts_nvalid indicates the number of valid columns in the tts_values/isnull
 * arrays.	When the slot is holding a "virtual" tuple this must be equal
 * to the descriptor's natts.  When the slot is holding a physical tuple
 * this is equal to the number of columns we have extracted (we always
 * extract columns from left to right, so there are no holes).
 *
 * tts_values/tts_isnull are allocated when a descriptor is assigned to the
 * slot; they are of length equal to the descriptor's natts.
 *
 * tts_mintuple must always be NULL if the slot does not hold a "minimal"
 * tuple.  When it does, tts_mintuple points to the actual MinimalTupleData
 * object (the thing to be pfree'd if tts_shouldFreeMin is true).  If the slot
 * has only a minimal and not also a regular physical tuple, then tts_tuple
 * points at tts_minhdr and the fields of that struct are set correctly
 * for access to the minimal tuple; in particular, tts_minhdr.t_data points
 * MINIMAL_TUPLE_OFFSET bytes before tts_mintuple.	This allows column
 * extraction to treat the case identically to regular physical tuples.
 *
 * tts_slow/tts_off are saved state for slot_deform_tuple, and should not
 * be touched by any other code.
 *----------
 */

/* tts_flags */
#define         TTS_ISEMPTY     1
#define         TTS_SHOULDFREE 	2
#define         TTS_SHOULDFREE_MEM 	4	/* should pfree tts_memtuple? */
#define         TTS_VIRTUAL     8

typedef struct TupleTableSlot
{
	NodeTag		type;
	int         PRIVATE_tts_flags;      /* TTS_xxx flags */

	/* Heap tuple stuff */
	HeapTuple PRIVATE_tts_heaptuple;
	void    *PRIVATE_tts_htup_buf;
	uint32   PRIVATE_tts_htup_buf_len;

	/* Mem tuple stuff */
	MemTuple PRIVATE_tts_memtuple;
	void 	*PRIVATE_tts_mtup_buf;
	uint32  PRIVATE_tts_mtup_buf_len;
	ItemPointerData PRIVATE_tts_synthetic_ctid;	/* needed if memtuple is stored on disk */
	
	/* Virtual tuple stuff */
	int 	PRIVATE_tts_nvalid;		/* number of valid virtual tup entries */
	long    PRIVATE_tts_off; 		/* hack to remember state of last decoding. */
	bool    PRIVATE_tts_slow; 		/* hack to remember state of last decoding. */
	Datum 	*PRIVATE_tts_values;		/* virtual tuple values */
	bool 	*PRIVATE_tts_isnull;		/* virtual tuple nulls */

	TupleDesc	tts_tupleDescriptor;	/* slot's tuple descriptor */
	MemTupleBinding *tts_mt_bind;		/* mem tuple's binding */ 
	MemoryContext 	tts_mcxt;		/* slot itself is in this context */
	Buffer		tts_buffer;		/* tuple's buffer, or InvalidBuffer */

    /* System attributes */
    Oid         tts_tableOid;
} TupleTableSlot;

static inline bool TupIsNull(TupleTableSlot *slot)
{
	return (slot == NULL || (slot->PRIVATE_tts_flags & TTS_ISEMPTY) != 0);
}
static inline void TupClearIsEmpty(TupleTableSlot *slot)
{
	slot->PRIVATE_tts_flags &= (~TTS_ISEMPTY);
}
static inline bool TupShouldFree(TupleTableSlot *slot)
{
	Assert(slot);
	return ((slot->PRIVATE_tts_flags & TTS_SHOULDFREE) != 0); 
}
static inline void TupSetShouldFree(TupleTableSlot *slot)
{
	slot->PRIVATE_tts_flags |= TTS_SHOULDFREE; 
}
static inline void TupClearShouldFree(TupleTableSlot *slot)
{
	slot->PRIVATE_tts_flags &= (~TTS_SHOULDFREE);
}
static inline bool TupHasHeapTuple(TupleTableSlot *slot)
{
	Assert(slot);
	return slot->PRIVATE_tts_heaptuple != NULL;
}
static inline bool TupHasMemTuple(TupleTableSlot *slot)
{
	Assert(slot);
	return slot->PRIVATE_tts_memtuple != NULL;
}
static inline bool TupHasVirtualTuple(TupleTableSlot *slot)
{
	Assert(slot);
    return (slot->PRIVATE_tts_flags & TTS_VIRTUAL) ? true : false;
}
static inline HeapTuple TupGetHeapTuple(TupleTableSlot *slot)
{
	Assert(TupHasHeapTuple(slot));
	return slot->PRIVATE_tts_heaptuple; 
}
static inline MemTuple TupGetMemTuple(TupleTableSlot *slot)
{
	Assert(TupHasMemTuple(slot));
	return slot->PRIVATE_tts_memtuple;
}

static inline void free_heaptuple_memtuple(TupleTableSlot *slot)
{
	if(TupShouldFree(slot))
	{
		if(slot->PRIVATE_tts_heaptuple && slot->PRIVATE_tts_heaptuple != slot->PRIVATE_tts_htup_buf)
			pfree(slot->PRIVATE_tts_heaptuple);
		
		if(slot->PRIVATE_tts_memtuple && slot->PRIVATE_tts_memtuple != slot->PRIVATE_tts_mtup_buf)
			pfree(slot->PRIVATE_tts_memtuple);
	}

	slot->PRIVATE_tts_heaptuple = NULL;
	slot->PRIVATE_tts_memtuple = NULL;
}

static inline void TupSetVirtualTuple(TupleTableSlot *slot)
{
	Assert(slot);
	slot->PRIVATE_tts_flags |= TTS_VIRTUAL;
}

static inline void TupSetVirtualTupleNValid(TupleTableSlot *slot, int nvalid)
{
        free_heaptuple_memtuple(slot);
        slot->PRIVATE_tts_flags = TTS_VIRTUAL;
        slot->PRIVATE_tts_nvalid = nvalid;
}

static inline Datum *slot_get_values(TupleTableSlot *slot)
{
	return slot->PRIVATE_tts_values;
}

static inline bool *slot_get_isnull(TupleTableSlot *slot)
{
	return slot->PRIVATE_tts_isnull;
}

extern void _slot_getsomeattrs(TupleTableSlot *slot, int attnum);
static inline void slot_getsomeattrs(TupleTableSlot *slot, int attnum)
{

	if(TupHasVirtualTuple(slot))
	{
		if(slot->PRIVATE_tts_nvalid >= attnum)
			return;
	}

	if(TupHasMemTuple(slot))
	{
		int i; 
		for(i=0; i<attnum; ++i)
			slot->PRIVATE_tts_values[i] = memtuple_getattr(
					slot->PRIVATE_tts_memtuple, slot->tts_mt_bind, 
					i+1, &(slot->PRIVATE_tts_isnull[i]));

		TupSetVirtualTuple(slot);
		slot->PRIVATE_tts_nvalid = attnum;
		return;
	}

	_slot_getsomeattrs(slot, attnum); 
	TupSetVirtualTuple(slot);
}

static inline void
slot_getallattrs(TupleTableSlot *slot)
{
	slot_getsomeattrs(slot, slot->tts_tupleDescriptor->natts);
}

extern Datum slot_getsysattr(TupleTableSlot *slot, int attnum, bool *isnull);

/*
 * Set the synthetic ctid to a given ctid value.
 *
 * If the slot contains a memtuple or a virtual tuple, the PRIVATE_tts_synthetic_ctid
 * is set to the given ctid value. If the slot contains a heap tuple, then t_self
 * in the heap tuple is set to the given ctid value.
 *
 * This function will set both PRIVATE_tts_synthetic_ctid and heaptuple->t_self when
 * both heap tuple and memtuple or virtual tuple are presented.
 */
static inline void slot_set_ctid(TupleTableSlot *slot, ItemPointer new_ctid)
{
	Assert(slot);

	if (TupHasHeapTuple(slot))
	{
		HeapTuple tuple = TupGetHeapTuple(slot);
		tuple->t_self = *new_ctid;
	}
	
	if (TupHasMemTuple(slot) || TupHasVirtualTuple(slot))
		slot->PRIVATE_tts_synthetic_ctid = *new_ctid;
}

extern void slot_set_ctid_from_fake(TupleTableSlot *slot, ItemPointerData *fake_ctid);

/*
 * Retrieve the synthetic ctid value from the slot.
 *
 * This function assumes that the PRIVATE_tts_synthetic_ctid and heaptuple->t_self
 * are in sync if both of them are presented in the slot.
 */
static inline ItemPointer slot_get_ctid(TupleTableSlot *slot)
{
	if (TupHasHeapTuple(slot))
	{
		Assert(ItemPointerIsValid(&(slot->PRIVATE_tts_heaptuple->t_self)));
		return &(slot->PRIVATE_tts_heaptuple->t_self);
	}
	
	return &(slot->PRIVATE_tts_synthetic_ctid);
}

/*
 * Get an attribute from the tuple table slot.
 */
static inline Datum slot_getattr(TupleTableSlot *slot, int attnum, bool *isnull)
{
	Assert(!TupIsNull(slot));
	Assert(attnum <= slot->tts_tupleDescriptor->natts);

	/* System attribute */
	if(attnum <= 0)
		return slot_getsysattr(slot, attnum, isnull);

	/* fast path for virtual tuple */
	if(TupHasVirtualTuple(slot) && slot->PRIVATE_tts_nvalid >= attnum)
	{
		*isnull = slot->PRIVATE_tts_isnull[attnum-1];
		return slot->PRIVATE_tts_values[attnum-1];
	}

	/* Mem tuple: We do not even populate virtual tuple */
	if(TupHasMemTuple(slot))
	{
		Assert(slot->tts_mt_bind);
		return memtuple_getattr(slot->PRIVATE_tts_memtuple, slot->tts_mt_bind, attnum, isnull);
	}

	/* Slow: heap tuple */
	Assert(TupHasHeapTuple(slot));

	_slot_getsomeattrs(slot, attnum);
	Assert(TupHasVirtualTuple(slot) && slot->PRIVATE_tts_nvalid >= attnum);
	*isnull = slot->PRIVATE_tts_isnull[attnum-1];
	return slot->PRIVATE_tts_values[attnum-1];
}

static inline bool slot_attisnull(TupleTableSlot *slot, int attnum)
{
	if(attnum <= 0)
		return false;

	if(TupHasHeapTuple(slot))
		return heap_attisnull_normalattr(slot->PRIVATE_tts_heaptuple, attnum);
		
	if(TupHasVirtualTuple(slot) && slot->PRIVATE_tts_nvalid >= attnum)
		return slot->PRIVATE_tts_isnull[attnum-1];

	Assert(TupHasMemTuple(slot));
	return memtuple_attisnull(slot->PRIVATE_tts_memtuple, slot->tts_mt_bind, attnum);
}

#ifdef GPDB_83_MERGE_FIXME
#define TTS_HAS_PHYSICAL_TUPLE(slot)  \
	((slot)->tts_tuple != NULL && (slot)->tts_tuple != &((slot)->tts_minhdr))
#endif

/* in executor/execTuples.c */
extern void init_slot(TupleTableSlot *slot, TupleDesc tupdesc);

extern TupleTableSlot *MakeTupleTableSlot(void);
extern TupleTableSlot *ExecAllocTableSlot(List **tupleTable);
extern void ExecResetTupleTable(List *tupleTable, bool shouldFree);
extern TupleTableSlot *MakeSingleTupleTableSlot(TupleDesc tupdesc);
extern void ExecDropSingleTupleTableSlot(TupleTableSlot *slot);
extern void ExecSetSlotDescriptor(TupleTableSlot *slot, TupleDesc tupdesc); 

extern TupleTableSlot *ExecStoreHeapTuple(HeapTuple tuple,
			   TupleTableSlot *slot,
			   Buffer buffer,
			   bool shouldFree);
extern TupleTableSlot *ExecStoreMinimalTuple(MemTuple mtup,
					  TupleTableSlot *slot,
					  bool shouldFree);

extern TupleTableSlot *ExecClearTuple(TupleTableSlot *slot);
extern TupleTableSlot *ExecStoreVirtualTuple(TupleTableSlot *slot);
extern TupleTableSlot *ExecStoreAllNullTuple(TupleTableSlot *slot);

extern HeapTuple ExecCopySlotHeapTuple(TupleTableSlot *slot);
extern MemTuple ExecCopySlotMemTuple(TupleTableSlot *slot);
extern MemTuple ExecCopySlotMemTupleTo(TupleTableSlot *slot, MemoryContext pctxt, char *dest, unsigned int *len);

extern HeapTuple ExecFetchSlotHeapTuple(TupleTableSlot *slot);
extern MemTuple ExecFetchSlotMemTuple(TupleTableSlot *slot, bool inline_toast);
extern Datum ExecFetchSlotTupleDatum(TupleTableSlot *slot);

static inline GenericTuple
ExecFetchSlotGenericTuple(TupleTableSlot *slot, bool mtup_inline_toast)
{
	Assert(!TupIsNull(slot));
	if (slot->PRIVATE_tts_memtuple == NULL && slot->PRIVATE_tts_heaptuple != NULL)
		return (GenericTuple) slot->PRIVATE_tts_heaptuple;

	return (GenericTuple) ExecFetchSlotMemTuple(slot, mtup_inline_toast);
}

static inline TupleTableSlot *
ExecStoreGenericTuple(GenericTuple tup, TupleTableSlot *slot, bool shouldFree)
{
	if (is_memtuple(tup))
		return ExecStoreMinimalTuple((MemTuple) tup, slot, shouldFree);

	return ExecStoreHeapTuple((HeapTuple) tup, slot, InvalidBuffer, shouldFree);
}

static inline GenericTuple
ExecCopyGenericTuple(TupleTableSlot *slot)
{
	Assert(!TupIsNull(slot));
	if(slot->PRIVATE_tts_heaptuple != NULL && slot->PRIVATE_tts_memtuple == NULL)
		return (GenericTuple) ExecCopySlotHeapTuple(slot);
	return (GenericTuple) ExecCopySlotMemTuple(slot);
}

extern HeapTuple ExecMaterializeSlot(TupleTableSlot *slot);
extern TupleTableSlot *ExecCopySlot(TupleTableSlot *dstslot, TupleTableSlot *srcslot);

extern void ExecModifyMemTuple(TupleTableSlot *slot, Datum *values, bool *isnull, bool *doRepl);

#endif   /* TUPTABLE_H */

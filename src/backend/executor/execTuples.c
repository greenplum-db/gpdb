/*-------------------------------------------------------------------------
 *
 * execTuples.c
 *	  Routines dealing with TupleTableSlots.  These are used for resource
 *	  management associated with tuples (eg, releasing buffer pins for
 *	  tuples in disk buffers, or freeing the memory occupied by transient
 *	  tuples).  Slots also provide access abstraction that lets us implement
 *	  "virtual" tuples to reduce data-copying overhead.
 *
 *	  Routines dealing with the type information for tuples. Currently,
 *	  the type information for a tuple is an array of FormData_pg_attribute.
 *	  This information is needed by routines manipulating tuples
 *	  (getattribute, formtuple, etc.).
 *
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/execTuples.c
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *
 *	 SLOT CREATION/DESTRUCTION
 *		MakeTupleTableSlot		- create an empty slot
 *		ExecAllocTableSlot		- create a slot within a tuple table
 *		ExecResetTupleTable		- clear and optionally delete a tuple table
 *		MakeSingleTupleTableSlot - make a standalone slot, set its descriptor
 *		ExecDropSingleTupleTableSlot - destroy a standalone slot
 *
 *	 SLOT ACCESSORS
 *		ExecSetSlotDescriptor	- set a slot's tuple descriptor
 *		ExecStoreTuple			- store a physical tuple in the slot
 *		ExecStoreMinimalTuple	- store a minimal physical tuple in the slot
 *		ExecClearTuple			- clear contents of a slot
 *		ExecStoreVirtualTuple	- mark slot as containing a virtual tuple
 *		ExecCopySlotTuple		- build a physical tuple from a slot
 *		ExecCopySlotMinimalTuple - build a minimal physical tuple from a slot
 *		ExecMaterializeSlot		- convert virtual to physical storage
 *		ExecCopySlot			- copy one slot's contents to another
 *
 *	 CONVENIENCE INITIALIZATION ROUTINES
 *		ExecInitResultTupleSlot    \	convenience routines to initialize
 *		ExecInitScanTupleSlot		\	the various tuple slots for nodes
 *		ExecInitExtraTupleSlot		/	which store copies of tuples.
 *		ExecInitNullTupleSlot	   /
 *
 *	 Routines that probably belong somewhere else:
 *		ExecTypeFromTL			- form a TupleDesc from a target list
 *
 *	 EXAMPLE OF HOW TABLE ROUTINES WORK
 *		Suppose we have a query such as SELECT emp.name FROM emp and we have
 *		a single SeqScan node in the query plan.
 *
 *		At ExecutorStart()
 *		----------------
 *		- ExecInitSeqScan() calls ExecInitScanTupleSlot() and
 *		  ExecInitResultTupleSlot() to construct TupleTableSlots
 *		  for the tuples returned by the access methods and the
 *		  tuples resulting from performing target list projections.
 *
 *		During ExecutorRun()
 *		----------------
 *		- SeqNext() calls ExecStoreTuple() to place the tuple returned
 *		  by the access methods into the scan tuple slot.
 *
 *		- ExecSeqScan() calls ExecStoreTuple() to take the result
 *		  tuple from ExecProject() and place it into the result tuple slot.
 *
 *		- ExecutePlan() calls ExecSelect(), which passes the result slot
 *		  to printtup(), which uses slot_getallattrs() to extract the
 *		  individual Datums for printing.
 *
 *		At ExecutorEnd()
 *		----------------
 *		- EndPlan() calls ExecResetTupleTable() to clean up any remaining
 *		  tuples left over from executing the query.
 *
 *		The important thing to watch in the executor code is how pointers
 *		to the slots containing tuples are passed instead of the tuples
 *		themselves.  This facilitates the communication of related information
 *		(such as whether or not a tuple should be pfreed, what buffer contains
 *		this tuple, the tuple's tuple descriptor, etc).  It also allows us
 *		to avoid physically constructing projection tuples in many cases.
 */
#include "postgres.h"

#include "access/heapam.h"
#include "access/htup.h"
#include "access/htup_details.h"
#include "access/tuptoaster.h"
#include "funcapi.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "nodes/nodeFuncs.h"
#include "parser/parsetree.h"               /* rt_fetch() */
#include "storage/bufmgr.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/typcache.h"

#include "executor/execDynamicScan.h"
#include "cdb/cdbvars.h"                    /* GpIdentity.segindex */

static TupleDesc ExecTypeFromTLInternal(List *targetList,
					   bool hasoid, bool skipjunk);


/* ----------------------------------------------------------------
 *				  tuple table create/delete functions
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *		MakeTupleTableSlot
 *
 *		Basic routine to make an empty TupleTableSlot.
 * --------------------------------
 */
void init_slot(TupleTableSlot *slot, TupleDesc tupdesc)
{
    MemSet(slot, 0, sizeof(*slot));

	slot->type = T_TupleTableSlot;
	slot->PRIVATE_tts_flags = TTS_ISEMPTY;
	slot->tts_tupleDescriptor = tupdesc;
	slot->tts_mcxt = CurrentMemoryContext;
	slot->tts_buffer = InvalidBuffer;
}

static void cleanup_slot(TupleTableSlot *slot)
{
	ExecClearTuple(slot);

	if (slot->tts_mt_bind)
	{
		destroy_memtuple_binding(slot->tts_mt_bind);
		slot->tts_mt_bind = NULL;
	}

	if (slot->tts_tupleDescriptor)
		ReleaseTupleDesc(slot->tts_tupleDescriptor);

	if (slot->PRIVATE_tts_htup_buf)
	{
		pfree(slot->PRIVATE_tts_htup_buf);
		slot->PRIVATE_tts_htup_buf = NULL;
		slot->PRIVATE_tts_htup_buf_len = 0;
	}

	if (slot->PRIVATE_tts_mtup_buf)
	{
		pfree(slot->PRIVATE_tts_mtup_buf);
		slot->PRIVATE_tts_mtup_buf = NULL;
		slot->PRIVATE_tts_mtup_buf_len = 0;
	}

	if (slot->PRIVATE_tts_values)
	{
		pfree(slot->PRIVATE_tts_values);
		slot->PRIVATE_tts_values = NULL;
	}
	if (slot->PRIVATE_tts_isnull)
	{
		pfree(slot->PRIVATE_tts_isnull);
		slot->PRIVATE_tts_isnull = NULL;
	}
}

TupleTableSlot *
MakeTupleTableSlot(void)
{
	TupleTableSlot *slot = makeNode(TupleTableSlot);

	init_slot(slot, NULL);

	return slot;
}

/* --------------------------------
 *		ExecAllocTableSlot
 *
 *		Create a tuple table slot within a tuple table (which is just a List).
 * --------------------------------
 */
TupleTableSlot *
ExecAllocTableSlot(List **tupleTable)
{
	TupleTableSlot *slot = MakeTupleTableSlot();

	*tupleTable = lappend(*tupleTable, slot);

	return slot;
}

/* --------------------------------
 *		ExecResetTupleTable
 *
 *		This releases any resources (buffer pins, tupdesc refcounts)
 *		held by the tuple table, and optionally releases the memory
 *		occupied by the tuple table data structure.
 *		It is expected that this routine be called by EndPlan().
 * --------------------------------
 */
void
ExecResetTupleTable(List *tupleTable,	/* tuple table */
					bool shouldFree)	/* true if we should free memory */
{
	ListCell   *lc;

	foreach(lc, tupleTable)
	{
		TupleTableSlot *slot = (TupleTableSlot *) lfirst(lc);

		/* Sanity checks */
		Assert(IsA(slot, TupleTableSlot));

		/* Always release resources and reset the slot to empty */
		ExecClearTuple(slot);
		if (slot->tts_tupleDescriptor)
			cleanup_slot(slot);

		/* If shouldFree, release memory occupied by the slot itself */
		if (shouldFree)
			pfree(slot);
	}

	/* If shouldFree, release the list structure */
	if (shouldFree)
		list_free(tupleTable);
}

/* --------------------------------
 *		MakeSingleTupleTableSlot
 *
 *		This is a convenience routine for operations that need a
 *		standalone TupleTableSlot not gotten from the main executor
 *		tuple table.  It makes a single slot and initializes it
 *		to use the given tuple descriptor.
 * --------------------------------
 */
TupleTableSlot *
MakeSingleTupleTableSlot(TupleDesc tupdesc)
{
	TupleTableSlot *slot = MakeTupleTableSlot();

	ExecSetSlotDescriptor(slot, tupdesc);

	return slot;
}

/* --------------------------------
 *		ExecDropSingleTupleTableSlot
 *
 *		Release a TupleTableSlot made with MakeSingleTupleTableSlot.
 *		DON'T use this on a slot that's part of a tuple table list!
 * --------------------------------
 */
void
ExecDropSingleTupleTableSlot(TupleTableSlot *slot)
{
	/* This should match ExecResetTupleTable's processing of one slot */
	Assert(IsA(slot, TupleTableSlot));
	cleanup_slot(slot);
	pfree(slot);
}


/* ----------------------------------------------------------------
 *				  tuple table slot accessor functions
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *		ExecSetSlotDescriptor
 *
 *		This function is used to set the tuple descriptor associated
 *		with the slot's tuple.  The passed descriptor must have lifespan
 *		at least equal to the slot's.  If it is a reference-counted descriptor
 *		then the reference count is incremented for as long as the slot holds
 *		a reference.
 * --------------------------------
 */
void
ExecSetSlotDescriptor(TupleTableSlot *slot,		/* slot to change */
					  TupleDesc tupdesc)		/* new tuple descriptor */
{
	/* For safety, make sure slot is empty before changing it */
	cleanup_slot(slot);

	/*
	 * Install the new descriptor; if it's refcounted, bump its refcount.
	 */
	slot->tts_tupleDescriptor = tupdesc;
	PinTupleDesc(tupdesc);

	{
		/*
		 * Allocate Datum/isnull arrays of the appropriate size.  These must have
		 * the same lifetime as the slot, so allocate in the slot's own context.
		 */
		MemoryContext oldcontext = MemoryContextSwitchTo(slot->tts_mcxt);

		slot->tts_mt_bind = create_memtuple_binding(tupdesc);
		slot->PRIVATE_tts_values = (Datum *) palloc(tupdesc->natts * sizeof(Datum));
		slot->PRIVATE_tts_isnull = (bool *) palloc(tupdesc->natts * sizeof(bool));

		MemoryContextSwitchTo(oldcontext);
	}
}

/* --------------------------------
 *		ExecStoreTuple
 *
 *		This function is used to store a physical tuple into a specified
 *		slot in the tuple table.
 *
 *		tuple:	tuple to store
 *		slot:	slot to store it in
 *		buffer: disk buffer if tuple is in a disk page, else InvalidBuffer
 *		shouldFree: true if ExecClearTuple should pfree() the tuple
 *					when done with it
 *
 * If 'buffer' is not InvalidBuffer, the tuple table code acquires a pin
 * on the buffer which is held until the slot is cleared, so that the tuple
 * won't go away on us.
 *
 * shouldFree is normally set 'true' for tuples constructed on-the-fly.
 * It must always be 'false' for tuples that are stored in disk pages,
 * since we don't want to try to pfree those.
 *
 * Another case where it is 'false' is when the referenced tuple is held
 * in a tuple table slot belonging to a lower-level executor Proc node.
 * In this case the lower-level slot retains ownership and responsibility
 * for eventually releasing the tuple.  When this method is used, we must
 * be certain that the upper-level Proc node will lose interest in the tuple
 * sooner than the lower-level one does!  If you're not certain, copy the
 * lower-level tuple with heap_copytuple and let the upper-level table
 * slot assume ownership of the copy!
 *
 * Return value is just the passed-in slot pointer.
 *
 * NOTE: before PostgreSQL 8.1, this function would accept a NULL tuple
 * pointer and effectively behave like ExecClearTuple (though you could
 * still specify a buffer to pin, which would be an odd combination).
 * This saved a couple lines of code in a few places, but seemed more likely
 * to mask logic errors than to be really useful, so it's now disallowed.
 * --------------------------------
 */
TupleTableSlot *
ExecStoreHeapTuple(HeapTuple tuple,
			   TupleTableSlot *slot,
			   Buffer buffer,
			   bool shouldFree)
{
	/*
	 * sanity checks
	 */
	Assert(tuple != NULL);
	Assert(slot != NULL);
	Assert(slot->tts_tupleDescriptor != NULL);
	/* passing shouldFree=true for a tuple on a disk page is not sane */
	Assert(BufferIsValid(buffer) ? (!shouldFree) : true);

	Assert(!is_memtuple((GenericTuple) tuple));

	/*
	 * Free any old physical tuple belonging to the slot.
	 */
	free_heaptuple_memtuple(slot);

	/*
	 * Store the new tuple into the specified slot.
	 */

	/* Clear tts_flags, here isempty set to false */
	slot->PRIVATE_tts_flags = shouldFree ? TTS_SHOULDFREE : 0;

	/* store the tuple */
	slot->PRIVATE_tts_heaptuple = (void *) tuple;

	/* Mark extracted state invalid */
	slot->PRIVATE_tts_nvalid = 0;

	/*
	 * If tuple is on a disk page, keep the page pinned as long as we hold a
	 * pointer into it.  We assume the caller already has such a pin.
	 *
	 * This is coded to optimize the case where the slot previously held a
	 * tuple on the same disk page: in that case releasing and re-acquiring
	 * the pin is a waste of cycles.  This is a common situation during
	 * seqscans, so it's worth troubling over.
	 */
	if (slot->tts_buffer != buffer)
	{
		if (BufferIsValid(slot->tts_buffer))
			ReleaseBuffer(slot->tts_buffer);
		slot->tts_buffer = buffer;
		if (BufferIsValid(buffer))
			IncrBufferRefCount(buffer);
	}

	return slot;
}

/* --------------------------------
 *		ExecStoreMinimalTuple
 *
 *		Like ExecStoreTuple, but insert a "minimal" tuple into the slot.
 *
 * No 'buffer' parameter since minimal tuples are never stored in relations.
 * --------------------------------
 */
TupleTableSlot *
ExecStoreMinimalTuple(MemTuple mtup,
					  TupleTableSlot *slot,
					  bool shouldFree)
{
	/*
	 * sanity checks
	 */
	Assert(mtup != NULL);
	Assert(slot != NULL);
	Assert(slot->tts_tupleDescriptor != NULL);
	Assert(slot->tts_mt_bind != NULL);

	Assert(is_memtuple((GenericTuple) mtup));

	/*
	 * Free any old physical tuple belonging to the slot.
	 */
	free_heaptuple_memtuple(slot);
	
	/*
	 * Store the new tuple into the specified slot.
	 */

	/* Clear tts_flags, here isempty set to false */
	if(shouldFree)
		TupSetShouldFree(slot);
	else
		TupClearShouldFree(slot);

	slot->PRIVATE_tts_memtuple = mtup;

	TupClearIsEmpty(slot);
	slot->PRIVATE_tts_nvalid = 0;

	/*
	 * Drop the pin on the referenced buffer, if there is one.
	 */
	if (BufferIsValid(slot->tts_buffer))
		ReleaseBuffer(slot->tts_buffer);

	slot->tts_buffer = InvalidBuffer;
	return slot;
}

/* --------------------------------
 *		ExecFetchSlotTupleDatum
 *			Fetch the slot's tuple as a composite-type Datum.
 *
 *		The result is always freshly palloc'd in the caller's memory context.
 * --------------------------------
 */
Datum
ExecFetchSlotTupleDatum(TupleTableSlot *slot)
{
	HeapTuple	tup;
	TupleDesc	tupdesc;

	/* Fetch slot's contents in regular-physical-tuple form */
	tup = ExecFetchSlotHeapTuple(slot);
	tupdesc = slot->tts_tupleDescriptor;

	/* Convert to Datum form */
	return heap_copy_tuple_as_datum(tup, tupdesc);
}

/* --------------------------------
 *		ExecClearTuple
 *
 *		This function is used to clear out a slot in the tuple table.
 *
 *		NB: only the tuple is cleared, not the tuple descriptor (if any).
 * --------------------------------
 */
TupleTableSlot *				/* return: slot passed */
ExecClearTuple(TupleTableSlot *slot)	/* slot in which to store tuple */
{
	/*
	 * sanity checks
	 */
	Assert(slot != NULL);

	/*
	 * Free the old physical tuple if necessary.
	 */
	free_heaptuple_memtuple(slot);

	slot->PRIVATE_tts_flags = TTS_ISEMPTY;
	slot->PRIVATE_tts_nvalid = 0;

	/*
	 * Drop the pin on the referenced buffer, if there is one.
	 */
	if (BufferIsValid(slot->tts_buffer))
		ReleaseBuffer(slot->tts_buffer);

	slot->tts_buffer = InvalidBuffer;
	return slot;
}

/* --------------------------------
 *		ExecStoreVirtualTuple
 *			Mark a slot as containing a virtual tuple.
 *
 * The protocol for loading a slot with virtual tuple data is:
 *		* Call ExecClearTuple to mark the slot empty.
 *		* Store data into the Datum/isnull arrays.
 *		* Call ExecStoreVirtualTuple to mark the slot valid.
 * This is a bit unclean but it avoids one round of data copying.
 * --------------------------------
 */
TupleTableSlot *
ExecStoreVirtualTuple(TupleTableSlot *slot)
{
	/*
	 * sanity checks
	 */
	Assert(slot != NULL);
	Assert(slot->tts_tupleDescriptor != NULL);
	
	TupClearIsEmpty(slot);
	TupSetVirtualTuple(slot);

	slot->PRIVATE_tts_nvalid = slot->tts_tupleDescriptor->natts;
	return slot;
}

/* --------------------------------
 *		ExecStoreAllNullTuple
 *			Set up the slot to contain a null in every column.
 *
 * At first glance this might sound just like ExecClearTuple, but it's
 * entirely different: the slot ends up full, not empty.
 * --------------------------------
 */
TupleTableSlot *
ExecStoreAllNullTuple(TupleTableSlot *slot)
{
	/*
	 * sanity checks
	 */
	Assert(slot != NULL);
	Assert(slot->tts_tupleDescriptor != NULL);

	/* Clear any old contents */
	free_heaptuple_memtuple(slot);

	/*
	 * Drop the pin on the referenced buffer, if there is one.
	 */
	if (BufferIsValid(slot->tts_buffer))
		ReleaseBuffer(slot->tts_buffer);
	slot->tts_buffer = InvalidBuffer;

	/*
	 * Fill all the columns of the virtual tuple with nulls
	 */
	MemSet(slot->PRIVATE_tts_isnull, true,
		   slot->tts_tupleDescriptor->natts * sizeof(bool));

	slot->PRIVATE_tts_flags = TTS_VIRTUAL;
	slot->PRIVATE_tts_nvalid = slot->tts_tupleDescriptor->natts;

	return slot;
}

/* --------------------------------
 *		ExecCopySlotTuple
 *			Obtain a copy of a slot's regular physical tuple.  The copy is
 *			palloc'd in the current memory context.
 *			The slot itself is undisturbed.
 *
 *		This works even if the slot contains a virtual or minimal tuple;
 *		however the "system columns" of the result will not be meaningful.
 * --------------------------------
 */
HeapTuple
ExecCopySlotHeapTuple(TupleTableSlot *slot)
{
	/*
	 * sanity checks
	 */
	Assert(!TupIsNull(slot));

	if(slot->PRIVATE_tts_heaptuple)
		return heap_copytuple(slot->PRIVATE_tts_heaptuple);


	slot_getallattrs(slot);

	/*
	 * Otherwise we need to build a tuple from the Datum array.
	 */
	return heap_form_tuple(slot->tts_tupleDescriptor,
						   slot_get_values(slot),
						   slot_get_isnull(slot));
}

/* --------------------------------
 *		ExecCopySlotMinimalTuple
 *			Obtain a copy of a slot's minimal physical tuple.  The copy is
 *			palloc'd in the current memory context.
 *			The slot itself is undisturbed.
 * --------------------------------
 */
MemTuple
ExecCopySlotMemTuple(TupleTableSlot *slot)
{
	/*
	 * sanity checks
	 */
	Assert(!TupIsNull(slot));
	Assert(slot->tts_mt_bind);

	/*
	 * If we have a physical tuple then just copy it.  Prefer to copy
	 * tts_mintuple since that's a tad cheaper.
	 */
	if (slot->PRIVATE_tts_memtuple)
		return memtuple_copy_to(slot->PRIVATE_tts_memtuple, NULL, NULL);
	
	slot_getallattrs(slot);

	/*
	 * Otherwise we need to build a tuple from the Datum array.
	 */
	return memtuple_form_to(slot->tts_mt_bind, slot_get_values(slot), slot_get_isnull(slot), NULL, 0, false);
}

MemTuple
ExecCopySlotMemTupleTo(TupleTableSlot *slot, MemoryContext pctxt, char *dest, unsigned int *len)
{
	uint32 dumlen;
	MemTuple mtup = NULL;

	Assert(!TupIsNull(slot));
	Assert(slot->tts_mt_bind);

	if(!len)
		len = &dumlen;
	
	if (TupHasMemTuple(slot))
	{
		mtup = memtuple_copy_to(slot->PRIVATE_tts_memtuple, (MemTuple) dest, len);
		if(mtup || !pctxt)
			return mtup;

		mtup = (MemTuple) MemoryContextAlloc(pctxt, *len);
		mtup = memtuple_copy_to(slot->PRIVATE_tts_memtuple, mtup, len);
		Assert(mtup);

		return mtup;
	}

	slot_getallattrs(slot);
	mtup = memtuple_form_to(slot->tts_mt_bind, slot_get_values(slot), slot_get_isnull(slot), (MemTuple) dest, len, false);

	if(mtup || !pctxt)
		return mtup;
	mtup = (MemTuple) MemoryContextAlloc(pctxt, *len);
	mtup = memtuple_form_to(slot->tts_mt_bind, slot_get_values(slot), slot_get_isnull(slot), mtup, len, false);

	Assert(mtup);
	return mtup;
}
		
/* --------------------------------
 *		ExecFetchSlotTuple
 *			Fetch the slot's regular physical tuple.
 *
 *		If the slot contains a virtual tuple, we convert it to physical
 *		form.  The slot retains ownership of the physical tuple.
 *		If it contains a minimal tuple we convert to regular form and store
 *		that in addition to the minimal tuple (not instead of, because
 *		callers may hold pointers to Datums within the minimal tuple).
 *
 * The main difference between this and ExecMaterializeSlot() is that this
 * does not guarantee that the contained tuple is local storage.
 * Hence, the result must be treated as read-only.
 * --------------------------------
 */
HeapTuple
ExecFetchSlotHeapTuple(TupleTableSlot *slot)
{
	/*
	 * sanity checks
	 */
	Assert(!TupIsNull(slot));

	/*
	 * If we have a regular physical tuple then just return it.
	 */
	if(slot->PRIVATE_tts_heaptuple)
		return slot->PRIVATE_tts_heaptuple;

	/*
	 * Otherwise materialize the slot...
	 */
	return ExecMaterializeSlot(slot);
}

/* --------------------------------
 *		ExecFetchSlotMinimalTuple
 *			Fetch the slot's minimal physical tuple.
 *
 *		If the slot contains a virtual tuple, we convert it to minimal
 *		physical form.  The slot retains ownership of the minimal tuple.
 *		If it contains a regular tuple we convert to minimal form and store
 *		that in addition to the regular tuple (not instead of, because
 *		callers may hold pointers to Datums within the regular tuple).
 *
 * As above, the result must be treated as read-only.
 * --------------------------------
 */
MemTuple
ExecFetchSlotMemTuple(TupleTableSlot *slot)
{
	MemTuple newTuple;
	uint32 tuplen;

	Assert(!TupIsNull(slot));
	Assert(slot->tts_mt_bind);

	if(slot->PRIVATE_tts_memtuple)
		return slot->PRIVATE_tts_memtuple;

	slot_getallattrs(slot);

	tuplen = slot->PRIVATE_tts_mtup_buf_len;
	newTuple = memtuple_form_to(slot->tts_mt_bind, slot_get_values(slot), slot_get_isnull(slot),
				(MemTuple) slot->PRIVATE_tts_mtup_buf, &tuplen, false);

	if(!newTuple)
	{
		if(slot->PRIVATE_tts_mtup_buf)
			pfree(slot->PRIVATE_tts_mtup_buf);

		slot->PRIVATE_tts_mtup_buf = MemoryContextAlloc(slot->tts_mcxt, tuplen);
		slot->PRIVATE_tts_mtup_buf_len = tuplen;

		newTuple = memtuple_form_to(slot->tts_mt_bind, slot_get_values(slot), slot_get_isnull(slot),
			(MemTuple) slot->PRIVATE_tts_mtup_buf, &tuplen, false);
	}

	Assert(newTuple);
	slot->PRIVATE_tts_memtuple = newTuple;

	return newTuple;
}

/* --------------------------------
 *		ExecMaterializeSlot
 *			Force a slot into the "materialized" state.
 *
 *		This causes the slot's tuple to be a local copy not dependent on
 *		any external storage.  A pointer to the contained tuple is returned.
 *
 *		A typical use for this operation is to prepare a computed tuple
 *		for being stored on disk.  The original data may or may not be
 *		virtual, but in any case we need a private copy for heap_insert
 *		to scribble on.
 *
 *		Greenplum: this function is quite different from upstream because
 *		the TupleTableSlot, HeapTuple and Memory/Minimal tuple differ.
 * --------------------------------
 */
HeapTuple
ExecMaterializeSlot(TupleTableSlot *slot)
{
	uint32		tuplen;
	HeapTuple	htup;
	void	   *htup_buf;

	/*
	 * sanity checks
	 */
	Assert(!TupIsNull(slot));

	/*
	 * If we have a regular physical tuple, and it's locally palloc'd, we have
	 * nothing to do, else make a copy.
	 *
	 * Don't transform the heaptuple if possible, transforming heaptuples to
	 * other types will lose the system columns and waste computing resources.
	 */
	if (slot->PRIVATE_tts_heaptuple)
	{
		if (TupShouldFree(slot) || slot->PRIVATE_tts_heaptuple == slot->PRIVATE_tts_htup_buf)
		{
			return slot->PRIVATE_tts_heaptuple;
		}
		else
		{
			htup_buf = slot->PRIVATE_tts_htup_buf;
			tuplen = HEAPTUPLESIZE + slot->PRIVATE_tts_heaptuple->t_len;

			slot->PRIVATE_tts_htup_buf = (HeapTuple) MemoryContextAlloc(slot->tts_mcxt, tuplen);

			htup = heaptuple_copy_to(slot->PRIVATE_tts_heaptuple, slot->PRIVATE_tts_htup_buf, &tuplen);
			Assert(htup);

			if (htup_buf)
				pfree(htup_buf);

			slot->PRIVATE_tts_heaptuple = htup;
			slot->PRIVATE_tts_nvalid = 0;
			return htup;
		}
	}

	/*
	 * Otherwise, copy or build a physical tuple, and store it into the slot.
	 */

	/*
	 * Transform any tuple to a virtual tuple, system columns would be lost
	 * here since virtual tuples have no system columns.
	 */
	slot_getallattrs(slot);

	Assert(slot->PRIVATE_tts_nvalid == slot->tts_tupleDescriptor->natts);

	/* Form a heaptuple from the virtual tuple */
	tuplen = slot->PRIVATE_tts_htup_buf_len;
	htup = heaptuple_form_to(slot->tts_tupleDescriptor, slot_get_values(slot), slot_get_isnull(slot),
			slot->PRIVATE_tts_htup_buf, &tuplen);

	/* The buf space is not large enough */
	if (!htup)
	{
		/* enlarge the buffer and retry */
		if (slot->PRIVATE_tts_htup_buf)
			pfree(slot->PRIVATE_tts_htup_buf);
		slot->PRIVATE_tts_htup_buf = (HeapTuple) MemoryContextAlloc(slot->tts_mcxt, tuplen);
		slot->PRIVATE_tts_htup_buf_len = tuplen;

		htup = heaptuple_form_to(slot->tts_tupleDescriptor,
								 slot_get_values(slot),
								 slot_get_isnull(slot),
								 slot->PRIVATE_tts_htup_buf,
								 &tuplen);
		Assert(htup);
	}

	slot->PRIVATE_tts_heaptuple = htup;

	/*
	 * Mark extracted state invalid.  This is important because the slot is
	 * not supposed to depend any more on the previous external data; we
	 * mustn't leave any dangling pass-by-reference datums in PRIVATE_tts_values.
	 */
	slot->PRIVATE_tts_nvalid = 0;

	return htup;
}

/* --------------------------------
 *		ExecCopySlot
 *			Copy the source slot's contents into the destination slot.
 *
 *		The destination acquires a private copy that will not go away
 *		if the source is cleared.
 *
 *		The caller must ensure the slots have compatible tupdescs.
 * --------------------------------
 */
TupleTableSlot *
ExecCopySlot(TupleTableSlot *dstslot, TupleTableSlot *srcslot)
{
	Assert(!TupIsNull(srcslot));

	ExecClearTuple(dstslot);
	TupClearIsEmpty(dstslot);

	/* heap tuple stuff */
	if(srcslot->PRIVATE_tts_heaptuple && !srcslot->PRIVATE_tts_memtuple) {

		uint32 tuplen = dstslot->PRIVATE_tts_htup_buf_len;
		HeapTuple htup = heaptuple_copy_to(srcslot->PRIVATE_tts_heaptuple, dstslot->PRIVATE_tts_htup_buf, &tuplen);

		if(!htup)
		{
			dstslot->PRIVATE_tts_htup_buf = MemoryContextAlloc(dstslot->tts_mcxt, tuplen);
			dstslot->PRIVATE_tts_htup_buf_len = tuplen;

			htup = heaptuple_copy_to(srcslot->PRIVATE_tts_heaptuple, dstslot->PRIVATE_tts_htup_buf, &tuplen);
		}

		Assert(htup);
		dstslot->PRIVATE_tts_heaptuple = htup;
		dstslot->PRIVATE_tts_nvalid = 0;
	}
	else
	{
		uint32 tuplen = dstslot->PRIVATE_tts_mtup_buf_len;
		MemTuple mtup;

		Assert(srcslot->tts_mt_bind != NULL && dstslot->tts_mt_bind != NULL);

		mtup = ExecCopySlotMemTupleTo(srcslot, NULL, dstslot->PRIVATE_tts_mtup_buf, &tuplen);
		if(!mtup)
		{
			dstslot->PRIVATE_tts_mtup_buf = MemoryContextAlloc(dstslot->tts_mcxt, tuplen);
			dstslot->PRIVATE_tts_mtup_buf_len = tuplen;

			mtup = ExecCopySlotMemTupleTo(srcslot, NULL, dstslot->PRIVATE_tts_mtup_buf, &tuplen);
		}

		Assert(mtup);
		dstslot->PRIVATE_tts_memtuple = mtup;
		dstslot->PRIVATE_tts_nvalid = 0;
	}

	return dstslot;
}

/* XXX
 * This function is not very efficient.  We should detech if we can modify
 * the memtuple inline so no deform/form is needed
 */
void ExecModifyMemTuple(TupleTableSlot *slot, Datum *values, bool *isnull, bool *doRepl)
{
	int i;
	MemTuple mtup;
	uint32 tuplen;
	Assert(slot->PRIVATE_tts_memtuple);

	/* First, get all the attrs.  Note we set PRIVATE_tts_nvalid to 0
	 * so we force the attrs are from memtuple
	 */
	slot->PRIVATE_tts_nvalid = 0;
	slot_getallattrs(slot);
	
	/* Next, we construct a new memtuple, on the htup buf to avoid palloc */
	slot->PRIVATE_tts_heaptuple = NULL;
	for(i = 0; i<slot->tts_tupleDescriptor->natts; ++i)
	{
		if(doRepl[i])
		{
			slot->PRIVATE_tts_values[i] = values[i];
			slot->PRIVATE_tts_isnull[i] = isnull[i];
		}
	}

	tuplen = slot->PRIVATE_tts_htup_buf_len;
	mtup = memtuple_form_to(slot->tts_mt_bind, slot->PRIVATE_tts_values, slot->PRIVATE_tts_isnull,
			slot->PRIVATE_tts_htup_buf, &tuplen, false);
	if(!mtup)
	{
		slot->PRIVATE_tts_htup_buf = MemoryContextAlloc(slot->tts_mcxt, tuplen);
		slot->PRIVATE_tts_htup_buf_len = tuplen;

		mtup = memtuple_form_to(slot->tts_mt_bind, slot->PRIVATE_tts_values, slot->PRIVATE_tts_isnull,
			slot->PRIVATE_tts_htup_buf, &tuplen, false);

		Assert(mtup);
	}

	/* Check if we need to free this mem tuple */
	if(TupShouldFree(slot)
			&& slot->PRIVATE_tts_memtuple
			&& slot->PRIVATE_tts_memtuple != slot->PRIVATE_tts_mtup_buf
	  )
		pfree(slot->PRIVATE_tts_memtuple);

	slot->PRIVATE_tts_memtuple = mtup;
	/* swap mtup_buf and htup_buf stuff */

	mtup = (MemTuple) slot->PRIVATE_tts_mtup_buf;
	tuplen = slot->PRIVATE_tts_mtup_buf_len;

	slot->PRIVATE_tts_mtup_buf = slot->PRIVATE_tts_htup_buf;
	slot->PRIVATE_tts_mtup_buf_len = slot->PRIVATE_tts_htup_buf_len;
	slot->PRIVATE_tts_htup_buf = (void *) mtup;
	slot->PRIVATE_tts_htup_buf_len = tuplen;

	/* don't forget to reset PRIVATE_tts_nvalid, because we modified the memtuple */
	slot->PRIVATE_tts_nvalid = 0;
}


/* ----------------------------------------------------------------
 *				convenience initialization routines
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *		ExecInit{Result,Scan,Extra}TupleSlot
 *
 *		These are convenience routines to initialize the specified slot
 *		in nodes inheriting the appropriate state.  ExecInitExtraTupleSlot
 *		is used for initializing special-purpose slots.
 * --------------------------------
 */

/* ----------------
 *		ExecInitResultTupleSlot
 * ----------------
 */
void
ExecInitResultTupleSlot(EState *estate, PlanState *planstate)
{
	planstate->ps_ResultTupleSlot = ExecAllocTableSlot(&estate->es_tupleTable);
}

/* ----------------
 *		ExecInitScanTupleSlot
 *
 *      CDB: Only Scan operators should use this function.  Operators other
 *      than scans should use ExecInitExtraTupleSlot.
 *
 *      CDB: Some system-defined attributes are kept in the TupleTableSlot
 *      so they can be reused for each tuple.  They're initialized here.
 *      During execution, Var nodes referencing system-defined attrs can
 *      occur only in the targetlist and qual exprs attached to a scan
 *      operator.  Those exprs are evaluated taking their input from the 
 *      scan operator's Scan Tuple Slot.
 * ----------------
 */
void
ExecInitScanTupleSlot(EState *estate, ScanState *scanstate)
{
    TupleTableSlot *slot = ExecAllocTableSlot(&estate->es_tupleTable);
    Scan           *scan = (Scan *)scanstate->ps.plan;
    RangeTblEntry  *rtentry;

	scanstate->ss_ScanTupleSlot = slot;

    /* CDB: Does this look like a Scan operator? */
    Insist(scan->scanrelid > 0 &&
           scan->scanrelid <= list_length(estate->es_range_table));

    /* What kind of scan? */
    rtentry = rt_fetch(scan->scanrelid, estate->es_range_table);
    switch (rtentry->rtekind)
    {
        case RTE_RELATION:
            /* Set 'tableoid' sysattr to the Oid of baserel's pg_class row. */
			if (isDynamicScan(&scan->plan))
				slot->tts_tableOid = DynamicScan_GetTableOid(scanstate);
			else
				slot->tts_tableOid = rtentry->relid;
            break;

        default:
            break;
    }
}

/* ----------------
 *		ExecInitExtraTupleSlot
 * ----------------
 */
TupleTableSlot *
ExecInitExtraTupleSlot(EState *estate)
{
	return ExecAllocTableSlot(&estate->es_tupleTable);
}

/* ----------------
 *		ExecInitNullTupleSlot
 *
 * Build a slot containing an all-nulls tuple of the given type.
 * This is used as a substitute for an input tuple when performing an
 * outer join.
 * ----------------
 */
TupleTableSlot *
ExecInitNullTupleSlot(EState *estate, TupleDesc tupType)
{
	TupleTableSlot *slot = ExecInitExtraTupleSlot(estate);

	ExecSetSlotDescriptor(slot, tupType);

	return ExecStoreAllNullTuple(slot);
}

/* ----------------------------------------------------------------
 *		ExecTypeFromTL
 *
 *		Generate a tuple descriptor for the result tuple of a targetlist.
 *		(A parse/plan tlist must be passed, not an ExprState tlist.)
 *		Note that resjunk columns, if any, are included in the result.
 *
 *		Currently there are about 4 different places where we create
 *		TupleDescriptors.  They should all be merged, or perhaps
 *		be rewritten to call BuildDesc().
 * ----------------------------------------------------------------
 */
TupleDesc
ExecTypeFromTL(List *targetList, bool hasoid)
{
	return ExecTypeFromTLInternal(targetList, hasoid, false);
}

/* ----------------------------------------------------------------
 *		ExecCleanTypeFromTL
 *
 *		Same as above, but resjunk columns are omitted from the result.
 * ----------------------------------------------------------------
 */
TupleDesc
ExecCleanTypeFromTL(List *targetList, bool hasoid)
{
	return ExecTypeFromTLInternal(targetList, hasoid, true);
}

static TupleDesc
ExecTypeFromTLInternal(List *targetList, bool hasoid, bool skipjunk)
{
	TupleDesc	typeInfo;
	ListCell   *l;
	int			len;
	int			cur_resno = 1;

	if (skipjunk)
		len = ExecCleanTargetListLength(targetList);
	else
		len = ExecTargetListLength(targetList);
	typeInfo = CreateTemplateTupleDesc(len, hasoid);

	foreach(l, targetList)
	{
		TargetEntry *tle = lfirst(l);

		if (skipjunk && tle->resjunk)
			continue;
		TupleDescInitEntry(typeInfo,
						   cur_resno,
						   tle->resname,
						   exprType((Node *) tle->expr),
						   exprTypmod((Node *) tle->expr),
						   0);
		TupleDescInitEntryCollation(typeInfo,
									cur_resno,
									exprCollation((Node *) tle->expr));
		cur_resno++;
	}

	return typeInfo;
}

/*
 * ExecTypeFromExprList - build a tuple descriptor from a list of Exprs
 *
 * This is roughly like ExecTypeFromTL, but we work from bare expressions
 * not TargetEntrys.  No names are attached to the tupledesc's columns.
 */
TupleDesc
ExecTypeFromExprList(List *exprList)
{
	TupleDesc	typeInfo;
	ListCell   *lc;
	int			cur_resno = 1;

	typeInfo = CreateTemplateTupleDesc(list_length(exprList), false);

	foreach(lc, exprList)
	{
		Node	   *e = lfirst(lc);

		TupleDescInitEntry(typeInfo,
						   cur_resno,
						   NULL,
						   exprType(e),
						   exprTypmod(e),
						   0);
		TupleDescInitEntryCollation(typeInfo,
									cur_resno,
									exprCollation(e));
		cur_resno++;
	}

	return typeInfo;
}

/*
 * ExecTypeSetColNames - set column names in a TupleDesc
 *
 * Column names must be provided as an alias list (list of String nodes).
 *
 * For some callers, the supplied tupdesc has a named rowtype (not RECORD)
 * and it is moderately likely that the alias list matches the column names
 * already present in the tupdesc.  If we do change any column names then
 * we must reset the tupdesc's type to anonymous RECORD; but we avoid doing
 * so if no names change.
 */
void
ExecTypeSetColNames(TupleDesc typeInfo, List *namesList)
{
	bool		modified = false;
	int			colno = 0;
	ListCell   *lc;

	foreach(lc, namesList)
	{
		char	   *cname = strVal(lfirst(lc));
		Form_pg_attribute attr;

		/* Guard against too-long names list */
		if (colno >= typeInfo->natts)
			break;
		attr = typeInfo->attrs[colno++];

		/* Ignore empty aliases (these must be for dropped columns) */
		if (cname[0] == '\0')
			continue;

		/* Change tupdesc only if alias is actually different */
		if (strcmp(cname, NameStr(attr->attname)) != 0)
		{
			namestrcpy(&(attr->attname), cname);
			modified = true;
		}
	}

	/* If we modified the tupdesc, it's now a new record type */
	if (modified)
	{
		typeInfo->tdtypeid = RECORDOID;
		typeInfo->tdtypmod = -1;
	}
}

/*
 * BlessTupleDesc - make a completed tuple descriptor useful for SRFs
 *
 * Rowtype Datums returned by a function must contain valid type information.
 * This happens "for free" if the tupdesc came from a relcache entry, but
 * not if we have manufactured a tupdesc for a transient RECORD datatype.
 * In that case we have to notify typcache.c of the existence of the type.
 */
TupleDesc
BlessTupleDesc(TupleDesc tupdesc)
{
	if (tupdesc->tdtypeid == RECORDOID &&
		tupdesc->tdtypmod < 0)
		assign_record_type_typmod(tupdesc);

	return tupdesc;				/* just for notational convenience */
}

/*
 * TupleDescGetSlot - Initialize a slot based on the supplied tupledesc
 *
 * Note: this is obsolete; it is sufficient to call BlessTupleDesc on
 * the tupdesc.  We keep it around just for backwards compatibility with
 * existing user-written SRFs.
 */
TupleTableSlot *
TupleDescGetSlot(TupleDesc tupdesc)
{
	TupleTableSlot *slot;

	/* The useful work is here */
	BlessTupleDesc(tupdesc);

	/* Make a standalone slot */
	slot = MakeSingleTupleTableSlot(tupdesc);

	/* Return the slot */
	return slot;
}

/*
 * TupleDescGetAttInMetadata - Build an AttInMetadata structure based on the
 * supplied TupleDesc. AttInMetadata can be used in conjunction with C strings
 * to produce a properly formed tuple.
 */
AttInMetadata *
TupleDescGetAttInMetadata(TupleDesc tupdesc)
{
	int			natts = tupdesc->natts;
	int			i;
	Oid			atttypeid;
	Oid			attinfuncid;
	FmgrInfo   *attinfuncinfo;
	Oid		   *attioparams;
	int32	   *atttypmods;
	AttInMetadata *attinmeta;

	attinmeta = (AttInMetadata *) palloc(sizeof(AttInMetadata));

	/* "Bless" the tupledesc so that we can make rowtype datums with it */
	attinmeta->tupdesc = BlessTupleDesc(tupdesc);

	/*
	 * Gather info needed later to call the "in" function for each attribute
	 */
	attinfuncinfo = (FmgrInfo *) palloc0(natts * sizeof(FmgrInfo));
	attioparams = (Oid *) palloc0(natts * sizeof(Oid));
	atttypmods = (int32 *) palloc0(natts * sizeof(int32));

	for (i = 0; i < natts; i++)
	{
		/* Ignore dropped attributes */
		if (!tupdesc->attrs[i]->attisdropped)
		{
			atttypeid = tupdesc->attrs[i]->atttypid;
			getTypeInputInfo(atttypeid, &attinfuncid, &attioparams[i]);
			fmgr_info(attinfuncid, &attinfuncinfo[i]);
			atttypmods[i] = tupdesc->attrs[i]->atttypmod;
		}
	}
	attinmeta->attinfuncs = attinfuncinfo;
	attinmeta->attioparams = attioparams;
	attinmeta->atttypmods = atttypmods;

	return attinmeta;
}

/*
 * BuildTupleFromCStrings - build a HeapTuple given user data in C string form.
 * values is an array of C strings, one for each attribute of the return tuple.
 * A NULL string pointer indicates we want to create a NULL field.
 */
HeapTuple
BuildTupleFromCStrings(AttInMetadata *attinmeta, char **values)
{
	TupleDesc	tupdesc = attinmeta->tupdesc;
	int			natts = tupdesc->natts;
	Datum	   *dvalues;
	bool	   *nulls;
	int			i;
	HeapTuple	tuple;

	dvalues = (Datum *) palloc(natts * sizeof(Datum));
	nulls = (bool *) palloc(natts * sizeof(bool));

	/* Call the "in" function for each non-dropped attribute */
	for (i = 0; i < natts; i++)
	{
		if (!tupdesc->attrs[i]->attisdropped)
		{
			/* Non-dropped attributes */
			dvalues[i] = InputFunctionCall(&attinmeta->attinfuncs[i],
										   values[i],
										   attinmeta->attioparams[i],
										   attinmeta->atttypmods[i]);
			if (values[i] != NULL)
				nulls[i] = false;
			else
				nulls[i] = true;
		}
		else
		{
			/* Handle dropped attributes by setting to NULL */
			dvalues[i] = (Datum) 0;
			nulls[i] = true;
		}
	}

	/*
	 * Form a tuple
	 */
	tuple = heap_form_tuple(tupdesc, dvalues, nulls);

	/*
	 * Release locally palloc'd space.  XXX would probably be good to pfree
	 * values of pass-by-reference datums, as well.
	 */
	pfree(dvalues);
	pfree(nulls);

	return tuple;
}

/*
 * HeapTupleHeaderGetDatum - convert a HeapTupleHeader pointer to a Datum.
 *
 * This must *not* get applied to an on-disk tuple; the tuple should be
 * freshly made by heap_form_tuple or some wrapper routine for it (such as
 * BuildTupleFromCStrings).  Be sure also that the tupledesc used to build
 * the tuple has a properly "blessed" rowtype.
 *
 * Formerly this was a macro equivalent to PointerGetDatum, relying on the
 * fact that heap_form_tuple fills in the appropriate tuple header fields
 * for a composite Datum.  However, we now require that composite Datums not
 * contain any external TOAST pointers.  We do not want heap_form_tuple itself
 * to enforce that; more specifically, the rule applies only to actual Datums
 * and not to HeapTuple structures.  Therefore, HeapTupleHeaderGetDatum is
 * now a function that detects whether there are externally-toasted fields
 * and constructs a new tuple with inlined fields if so.  We still need
 * heap_form_tuple to insert the Datum header fields, because otherwise this
 * code would have no way to obtain a tupledesc for the tuple.
 *
 * Note that if we do build a new tuple, it's palloc'd in the current
 * memory context.  Beware of code that changes context between the initial
 * heap_form_tuple/etc call and calling HeapTuple(Header)GetDatum.
 *
 * For performance-critical callers, it could be worthwhile to take extra
 * steps to ensure that there aren't TOAST pointers in the output of
 * heap_form_tuple to begin with.  It's likely however that the costs of the
 * typcache lookup and tuple disassembly/reassembly are swamped by TOAST
 * dereference costs, so that the benefits of such extra effort would be
 * minimal.
 *
 * XXX it would likely be better to create wrapper functions that produce
 * a composite Datum from the field values in one step.  However, there's
 * enough code using the existing APIs that we couldn't get rid of this
 * hack anytime soon.
 */
Datum
HeapTupleHeaderGetDatum(HeapTupleHeader tuple)
{
	Datum		result;
	TupleDesc	tupDesc;

	/* No work if there are no external TOAST pointers in the tuple */
	if (!HeapTupleHeaderHasExternal(tuple))
		return PointerGetDatum(tuple);

	/* Use the type data saved by heap_form_tuple to look up the rowtype */
	tupDesc = lookup_rowtype_tupdesc(HeapTupleHeaderGetTypeId(tuple),
									 HeapTupleHeaderGetTypMod(tuple));

	/* And do the flattening */
	result = toast_flatten_tuple_to_datum(tuple,
										HeapTupleHeaderGetDatumLength(tuple),
										  tupDesc);

	ReleaseTupleDesc(tupDesc);

	return result;
}


/*
 * Functions for sending tuples to the frontend (or other specified destination)
 * as though it is a SELECT result. These are used by utility commands that
 * need to project directly to the destination and don't need or want full
 * table function capability. Currently used by EXPLAIN and SHOW ALL.
 */
TupOutputState *
begin_tup_output_tupdesc(DestReceiver *dest, TupleDesc tupdesc)
{
	TupOutputState *tstate;

	tstate = (TupOutputState *) palloc(sizeof(TupOutputState));

	tstate->slot = MakeSingleTupleTableSlot(tupdesc);
	tstate->dest = dest;

	(*tstate->dest->rStartup) (tstate->dest, (int) CMD_SELECT, tupdesc);

	return tstate;
}

/*
 * write a single tuple
 */
void
do_tup_output(TupOutputState *tstate, Datum *values, bool *isnull)
{
	TupleTableSlot *slot = tstate->slot;
	int			natts = slot->tts_tupleDescriptor->natts;

	/* make sure the slot is clear */
	ExecClearTuple(slot);

	/* insert data */
	memcpy(slot->PRIVATE_tts_values, values, natts * sizeof(Datum));
	memcpy(slot->PRIVATE_tts_isnull, isnull, natts * sizeof(bool));

	/* mark slot as containing a virtual tuple */
	ExecStoreVirtualTuple(slot);

	/* send the tuple to the receiver */
	(*tstate->dest->receiveSlot) (slot, tstate->dest);

	/* clean up */
	ExecClearTuple(slot);
}

/*
 * write a chunk of text, breaking at newline characters
 *
 * Should only be used with a single-TEXT-attribute tupdesc.
 */
void
do_text_output_multiline(TupOutputState *tstate, const char *txt)
{
	Datum		values[1];
	bool		isnull[1] = {false};

	while (*txt)
	{
		const char *eol;
		int			len;

		eol = strchr(txt, '\n');
		if (eol)
		{
			len = eol - txt;
			eol++;
		}
		else
		{
			len = strlen(txt);
			eol = txt + len;
		}

		values[0] = PointerGetDatum(cstring_to_text_with_len(txt, len));
		do_tup_output(tstate, values, isnull);
		pfree(DatumGetPointer(values[0]));
		txt = eol;
	}
}

void
end_tup_output(TupOutputState *tstate)
{
	(*tstate->dest->rShutdown) (tstate->dest);
	/* note that destroying the dest is not ours to do */
	ExecDropSingleTupleTableSlot(tstate->slot);
	pfree(tstate);
}

/*
 * Get a system attribute from the tuple table slot.
 */
bool
slot_getsysattr(TupleTableSlot *slot, int attnum, Datum *result, bool *isnull)
{
		Assert(attnum < 0);         /* else caller error */
		if (TupIsNull(slot))
			return false;

		*result = 0;
        /* Currently, no sys attribute ever reads as NULL. */
        if (isnull)
                *isnull = false;

        /* HeapTuple */
        if (slot->PRIVATE_tts_heaptuple)
        {
                HeapTuple   htup = slot->PRIVATE_tts_heaptuple;

                Assert(htup);
                switch(attnum)
                {
                        case SelfItemPointerAttributeNumber:
							Assert(ItemPointerIsValid(&(htup->t_self)));
							
							*result = PointerGetDatum(&(htup->t_self));
							break;
                        case ObjectIdAttributeNumber:
							*result = ObjectIdGetDatum(HeapTupleGetOid(htup));
							break;
                        case TableOidAttributeNumber:
							*result = ObjectIdGetDatum(slot->tts_tableOid);
							break;
                        default:
							*result = heap_getsysattr(htup, attnum, slot->tts_tupleDescriptor, isnull);
							break;
                }
        }

        /* MemTuple, virtual tuple */
        else
        {
			Assert(TupHasMemTuple(slot) || TupHasVirtualTuple(slot));
			
                switch(attnum)
                {
                        case SelfItemPointerAttributeNumber:
							Assert(ItemPointerIsValid(&(slot->PRIVATE_tts_synthetic_ctid)));
							*result = PointerGetDatum(&(slot->PRIVATE_tts_synthetic_ctid));
							break;
                        case ObjectIdAttributeNumber:
							if(slot->PRIVATE_tts_memtuple)
								*result = ObjectIdGetDatum(MemTupleGetOid(slot->PRIVATE_tts_memtuple,
																		 slot->tts_mt_bind));
							else
								*result = ObjectIdGetDatum(InvalidOid);
							break;
                        case GpSegmentIdAttributeNumber:
							*result = Int32GetDatum(GpIdentity.segindex);
							break;
                        case TableOidAttributeNumber:
							*result = ObjectIdGetDatum(slot->tts_tableOid);
							break;
                        default:
							elog(ERROR, "Invalid attnum: %d", attnum);
                }
        }

        return true;
}                               /* slot_getsysattr */

/*
 * Set a synthetic ctid based on a fake ctid. Fake ctid is incremented before
 * the assignment.
 */
void
slot_set_ctid_from_fake(TupleTableSlot *slot, ItemPointerData *fake_ctid)
{
	/* Uninitialized */
	if (!ItemPointerIsValid(fake_ctid))
	{
		ItemPointerSetBlockNumber(fake_ctid, 0);
		ItemPointerSetOffsetNumber(fake_ctid, FirstOffsetNumber);
	}
	else
	{
		/* would we overflow? */
		if (ItemPointerGetOffsetNumber(fake_ctid) ==
			MaxOffsetNumber - 1)
		{
			/* How can we overflow 2^46? */
			Assert(ItemPointerGetBlockNumber(fake_ctid) !=
				   MaxBlockNumber - 1);
			ItemPointerSetBlockNumber(fake_ctid,
					ItemPointerGetBlockNumber(fake_ctid) + 1);
			ItemPointerSetOffsetNumber(fake_ctid,
					FirstOffsetNumber);
		}
		else
		{
			ItemPointerSetOffsetNumber(fake_ctid,
				ItemPointerGetOffsetNumber(fake_ctid) + 1);
		}
	}

	slot_set_ctid(slot, fake_ctid);
}

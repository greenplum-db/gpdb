/*-------------------------------------------------------------------------
 *
 * combocid.c
 *	  Combo command ID support routines
 *
 * Before version 8.3, HeapTupleHeaderData had separate fields for cmin
 * and cmax.  To reduce the header size, cmin and cmax are now overlayed
 * in the same field in the header.  That usually works because you rarely
 * insert and delete a tuple in the same transaction, and we don't need
 * either field to remain valid after the originating transaction exits.
 * To make it work when the inserting transaction does delete the tuple,
 * we create a "combo" command ID and store that in the tuple header
 * instead of cmin and cmax. The combo command ID can be mapped to the
 * real cmin and cmax using a backend-private array, which is managed by
 * this module.
 *
 * To allow reusing existing combo cids, we also keep a hash table that
 * maps cmin,cmax pairs to combo cids.  This keeps the data structure size
 * reasonable in most cases, since the number of unique pairs used by any
 * one transaction is likely to be small.
 *
 * With a 32-bit combo command id we can represent 2^32 distinct cmin,cmax
 * combinations. In the most perverse case where each command deletes a tuple
 * generated by every previous command, the number of combo command ids
 * required for N commands is N*(N+1)/2.  That means that in the worst case,
 * that's enough for 92682 commands.  In practice, you'll run out of memory
 * and/or disk space way before you reach that limit.
 *
 * The array and hash table are kept in TopTransactionContext, and are
 * destroyed at the end of each transaction.
 *
 * GPDB: In addition to the local array and hash table, the QE writer process
 * also maintains a copy of the array in shared memory, in a DSM segment. QE
 * reader processes can access the writer's shared array to look up combo
 * CIDs.
 *
 * Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/time/combocid.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "miscadmin.h"
#include "access/htup_details.h"
#include "access/xact.h"
#include "storage/shmem.h"
#include "utils/combocid.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"

#include "cdb/cdbvars.h"
#include "storage/proc.h"
#include "storage/dsm.h"
#include "utils/resowner.h"

/* Hash table to lookup combo cids by cmin and cmax */
static HTAB *comboHash = NULL;

/* Key and entry structures for the hash table */
typedef struct
{
	CommandId	cmin;
	CommandId	cmax;
} ComboCidKeyData;

typedef ComboCidKeyData *ComboCidKey;

typedef struct
{
	ComboCidKeyData key;
	CommandId	combocid;
} ComboCidEntryData;

typedef ComboCidEntryData *ComboCidEntry;

/* Initial size of the hash table */
#define CCID_HASH_SIZE			100


/*
 * An array of cmin,cmax pairs, indexed by combo command id.
 * To convert a combo cid to cmin and cmax, you do a simple array lookup.
 */
static ComboCidKey comboCids = NULL;
static int	usedComboCids = 0;	/* number of elements in comboCids */
static int	sizeComboCids = 0;	/* allocated size of array */

/* Initial size of the array */
#define CCID_ARRAY_SIZE			100

/* prototypes for internal functions */
static CommandId GetComboCommandId(CommandId cmin, CommandId cmax);
static CommandId GetRealCmin(CommandId combocid);
static CommandId GetRealCmax(CommandId combocid);

/*
 * To shared the combocids array from QE writer to QE readers, we keep a
 * copy of the 'comboCids' array in a DSM segment. The DSM segment has
 * the same serialized format as used by Serialize/RestoreComboCIDState
 * functions: the segment begins with the number of elements as an 'int',
 * followed by the array of ComboCidKeys.
 *
 * The dumpSharedComboCommandIds() function updates shared memory copy with
 * any new entries in local 'comboCids' array, and loadSharedComboCommandIds()
 * loads the local array from the shared copy.
 */
static dsm_segment *shared_comboCids = NULL;
static int shared_usedComboCids = 0;
static int shared_sizeComboCids = 0;

static void dumpSharedComboCommandIds(void);
static void loadSharedComboCommandIds(void);

/**** External API ****/

/*
 * GetCmin and GetCmax assert that they are only called in situations where
 * they make sense, that is, can deliver a useful answer.  If you have
 * reason to examine a tuple's t_cid field from a transaction other than
 * the originating one, use HeapTupleHeaderGetRawCommandId() directly.
 */

CommandId
HeapTupleHeaderGetCmin(HeapTupleHeader tup)
{
	CommandId	cid = HeapTupleHeaderGetRawCommandId(tup);

	Assert(!(tup->t_infomask & HEAP_MOVED));
	Assert(TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmin(tup)));

	if (tup->t_infomask & HEAP_COMBOCID)
		return GetRealCmin(cid);
	else
		return cid;
}

CommandId
HeapTupleHeaderGetCmax(HeapTupleHeader tup)
{
	CommandId	cid = HeapTupleHeaderGetRawCommandId(tup);

	Assert(!(tup->t_infomask & HEAP_MOVED));

	/*
	 * Because GetUpdateXid() performs memory allocations if xmax is a
	 * multixact we can't Assert() if we're inside a critical section. This
	 * weakens the check, but not using GetCmax() inside one would complicate
	 * things too much.
	 */
	Assert(CritSectionCount > 0 ||
	  TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetUpdateXid(tup)));

	if (tup->t_infomask & HEAP_COMBOCID)
		return GetRealCmax(cid);
	else
		return cid;
}

/*
 * Given a tuple we are about to delete, determine the correct value to store
 * into its t_cid field.
 *
 * If we don't need a combo CID, *cmax is unchanged and *iscombo is set to
 * FALSE.  If we do need one, *cmax is replaced by a combo CID and *iscombo
 * is set to TRUE.
 *
 * The reason this is separate from the actual HeapTupleHeaderSetCmax()
 * operation is that this could fail due to out-of-memory conditions.  Hence
 * we need to do this before entering the critical section that actually
 * changes the tuple in shared buffers.
 */
void
HeapTupleHeaderAdjustCmax(HeapTupleHeader tup,
						  CommandId *cmax,
						  bool *iscombo)
{
	/*
	 * If we're marking a tuple deleted that was inserted by (any
	 * subtransaction of) our transaction, we need to use a combo command id.
	 * Test for HeapTupleHeaderXminCommitted() first, because it's cheaper
	 * than a TransactionIdIsCurrentTransactionId call.
	 */
	if (!HeapTupleHeaderXminCommitted(tup) &&
		TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetRawXmin(tup)))
	{
		CommandId	cmin = HeapTupleHeaderGetCmin(tup);

		*cmax = GetComboCommandId(cmin, *cmax);
		*iscombo = true;
	}
	else
	{
		*iscombo = false;
	}
}

/*
 * Combo command ids are only interesting to the inserting and deleting
 * transaction, so we can forget about them at the end of transaction.
 */
void
AtEOXact_ComboCid(void)
{
	/*
	 * Don't bother to pfree. These are allocated in TopTransactionContext, so
	 * they're going to go away at the end of transaction anyway.
	 */
	comboHash = NULL;

	comboCids = NULL;
	usedComboCids = 0;
	sizeComboCids = 0;
}


/**** Internal routines ****/

/*
 * Get a combo command id that maps to cmin and cmax.
 *
 * We try to reuse old combo command ids when possible.
 */
static CommandId
GetComboCommandId(CommandId cmin, CommandId cmax)
{
	CommandId	combocid;
	ComboCidKeyData key;
	ComboCidEntry entry;
	bool		found;

	if (Gp_role == GP_ROLE_EXECUTE && !Gp_is_writer)
	{
		if (IS_QUERY_DISPATCHER())
			elog(ERROR, "EntryReader qExec tried to allocate a Combo Command Id");
		else
			elog(ERROR, "Reader qExec tried to allocate a Combo Command Id");
	}

	/* We're either GP_ROLE_DISPATCH, GP_ROLE_UTILITY, or a QE-writer */

	/*
	 * Create the hash table and array the first time we need to use combo
	 * cids in the transaction.
	 */
	if (comboHash == NULL)
	{
		HASHCTL		hash_ctl;

		/* Make array first; existence of hash table asserts array exists */
		comboCids = (ComboCidKeyData *)
			MemoryContextAlloc(TopTransactionContext,
							   sizeof(ComboCidKeyData) * CCID_ARRAY_SIZE);
		sizeComboCids = CCID_ARRAY_SIZE;
		usedComboCids = 0;

		memset(&hash_ctl, 0, sizeof(hash_ctl));
		hash_ctl.keysize = sizeof(ComboCidKeyData);
		hash_ctl.entrysize = sizeof(ComboCidEntryData);
		hash_ctl.hcxt = TopTransactionContext;

		comboHash = hash_create("Combo CIDs",
								CCID_HASH_SIZE,
								&hash_ctl,
								HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);
	}

	/*
	 * Grow the array if there's not at least one free slot.  We must do this
	 * before possibly entering a new hashtable entry, else failure to
	 * repalloc would leave a corrupt hashtable entry behind.
	 */
	if (usedComboCids >= sizeComboCids)
	{
		int			newsize = sizeComboCids * 2;

		comboCids = (ComboCidKeyData *)
			repalloc(comboCids, sizeof(ComboCidKeyData) * newsize);
		sizeComboCids = newsize;
	}

	/* Lookup or create a hash entry with the desired cmin/cmax */

	/* We assume there is no struct padding in ComboCidKeyData! */
	key.cmin = cmin;
	key.cmax = cmax;
	entry = (ComboCidEntry) hash_search(comboHash,
										(void *) &key,
										HASH_ENTER,
										&found);

	if (found)
	{
		/* Reuse an existing combo cid */
		return entry->combocid;
	}

	/* We have to create a new combo cid; we already made room in the array */
	combocid = usedComboCids;

	comboCids[combocid].cmin = cmin;
	comboCids[combocid].cmax = cmax;
	usedComboCids++;

	entry->combocid = combocid;

	/*
	 * If we're the QE writer or the dispatcher, share the new combo CID with
	 * readers. (In utility mode, no need to share.)
	 */
	if (Gp_role != GP_ROLE_UTILITY)
		dumpSharedComboCommandIds();

	return combocid;
}

static CommandId
GetRealCmin(CommandId combocid)
{
	/*
	 * If we are a reader process, check if we need to update our private copy
	 * of the shared comboCids first.
	 */
	if (combocid >= usedComboCids && Gp_role == GP_ROLE_EXECUTE && !Gp_is_writer)
		loadSharedComboCommandIds();

	if (combocid >= usedComboCids)
		elog(ERROR, "GetRealCmin: no combocid entry found for combo cid %u/%u", combocid, usedComboCids);
	return comboCids[combocid].cmin;
}

static CommandId
GetRealCmax(CommandId combocid)
{
	/*
	 * If we are a reader process, check if we need to update our private copy
	 * of the shared comboCids first.
	 */
	if (combocid >= usedComboCids && Gp_role == GP_ROLE_EXECUTE && !Gp_is_writer)
		loadSharedComboCommandIds();

	if (combocid >= usedComboCids)
		elog(ERROR, "GetRealCmax: no combocid entry found for combo cid %u/%u", combocid, usedComboCids);
	return comboCids[combocid].cmax;
}

/*
 * Estimate the amount of space required to serialize the current ComboCID
 * state.
 */
Size
EstimateComboCIDStateSpace(void)
{
	Size		size;

	/* Add space required for saving usedComboCids */
	size = sizeof(int);

	/* Add space required for saving the combocids key */
	size = add_size(size, mul_size(sizeof(ComboCidKeyData), usedComboCids));

	return size;
}

/*
 * Serialize the ComboCID state into the memory, beginning at start_address.
 * maxsize should be at least as large as the value returned by
 * EstimateComboCIDStateSpace.
 */
void
SerializeComboCIDState(Size maxsize, char *start_address)
{
	char	   *endptr;

	/* First, we store the number of currently-existing ComboCIDs. */
	*(int *) start_address = usedComboCids;

	/* If maxsize is too small, throw an error. */
	endptr = start_address + sizeof(int) +
		(sizeof(ComboCidKeyData) * usedComboCids);
	if (endptr < start_address || endptr > start_address + maxsize)
		elog(ERROR, "not enough space to serialize ComboCID state");

	/* Now, copy the actual cmin/cmax pairs. */
	if (usedComboCids > 0)
		memcpy(start_address + sizeof(int), comboCids,
			   (sizeof(ComboCidKeyData) * usedComboCids));
}

/*
 * Read the ComboCID state at the specified address and initialize this
 * backend with the same ComboCIDs.  This is only valid in a backend that
 * currently has no ComboCIDs (and only makes sense if the transaction state
 * is serialized and restored as well).
 */
void
RestoreComboCIDState(char *comboCIDstate)
{
	int			num_elements;
	ComboCidKeyData *keydata;
	int			i;
	CommandId	cid;

	Assert(!comboCids && !comboHash);

	/* First, we retrieve the number of ComboCIDs that were serialized. */
	num_elements = *(int *) comboCIDstate;
	keydata = (ComboCidKeyData *) (comboCIDstate + sizeof(int));

	/* Use GetComboCommandId to restore each ComboCID. */
	for (i = 0; i < num_elements; i++)
	{
		cid = GetComboCommandId(keydata[i].cmin, keydata[i].cmax);

		/* Verify that we got the expected answer. */
		if (cid != i)
			elog(ERROR, "unexpected command ID while restoring combo CIDs");
	}
}

/*
 * Copy the local comboCids array into shared memory, so that it can be
 * accessed by QE reader processes.
 *
 * In any given segment, there are many readers, but only one writer. The
 * writer process maintains an array of combo CIDs like in PostgreSQL,
 * but in addition to the local array, it maintains a copy of it in shared
 * memory, as a DSM segment. The handle of the DSM segment is made available
 * to reader processes in MyProc->comboCidsHandle.
 *
 * This function copies the local comboCids array to the DSM segment,
 * reallocating a larger DSM segment if needed. Since combo cid entries are
 * always appended to the end of a combo cid dsm segment, and because there is
 * only one writer, it is not necessary to lock the combo cid DSM segment
 * during reading or writing. A new combo cid will not become visible to the
 * reader until we have incremented the count stored in the beginning of the
 * DSM segment. We have to be careful with memory ordering, though, to make
 * sure the new entry becomes visible to readers before the counter is
 * incremented!
 *
 * The reader processes can find the current DSM segment via lockHolderProcPtr.
 */
void
dumpSharedComboCommandIds(void)
{
	char	   *shared_ptr;
	int		   *num_elements_ptr;
	ComboCidKey keydata;
	dsm_segment *oldsegment = shared_comboCids;

	Assert(Gp_role != GP_ROLE_EXECUTE || Gp_is_writer);

	/*
	 * Allocate/extend the shared array, if the new elements don't fit in the
	 * old one.
	 */
	if (usedComboCids > shared_sizeComboCids)
	{
		ResourceOwner oldowner;
		dsm_segment *newsegment;

		oldowner = CurrentResourceOwner;
		CurrentResourceOwner = TopTransactionResourceOwner;

		/*
		 * DSM segments cannot be resized, so we have to allocate a whole new
		 * segment.
		 * Create a new DSM segment for the combocids array. If we had an
		 * old one, we'll copy it over to the new array. (DSM segments
		 * cannot be resized.)
		 */
		newsegment = dsm_create(sizeof(int) + sizeof(ComboCidKeyData) * sizeComboCids,
								DSM_CREATE_NULL_IF_MAXSEGMENTS);
		if (newsegment == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 errmsg("could not create DSM segment for %d combo CIDs",
							sizeComboCids)));
		/*
		 * let current ResourceOwner forget this dsm
		 * and manage the lifecycle by ourselves
		 */
		dsm_pin_mapping(newsegment);

		shared_comboCids = newsegment;

		/* update dsm size */
		shared_sizeComboCids = sizeComboCids;

		/* reset current usage amount */
		shared_usedComboCids = 0;
		shared_ptr = dsm_segment_address(shared_comboCids);
		num_elements_ptr = (int *) shared_ptr;
		*num_elements_ptr = 0;

		CurrentResourceOwner = oldowner;
	}

	/*
	 * Copy all new entries to the shared array. (This function is called
	 * after each combocid assignment, so in practice there should always
	 * be exactly one new one).
	 */
	shared_ptr = dsm_segment_address(shared_comboCids);
	num_elements_ptr = (int *) shared_ptr;
	Assert(*num_elements_ptr == shared_usedComboCids);
	keydata = (ComboCidKeyData *) (shared_ptr + sizeof(int));

	for (int i = shared_usedComboCids; i < usedComboCids; i++)
		keydata[i] = comboCids[i];

	/*
	 * Finally, advertise the new count. We need a memory barrier to make sure
	 * that the array contents become visible to other backends before the
	 * count!
	 */
	pg_write_barrier();
	*num_elements_ptr = usedComboCids;
	shared_usedComboCids = usedComboCids;

	/*
	 * If we had to allocate a new segment, we swap it in place and release the
	 * old one now.
	 */
	if (oldsegment != shared_comboCids)
	{
		MyProc->comboCidsHandle = dsm_segment_handle(shared_comboCids);
		if (oldsegment)
			dsm_detach(oldsegment);
	}
}

/*
 * Load the comboCids array from shared memory.
 */
static void
loadSharedComboCommandIds(void)
{
	dsm_segment *attached_comboCids;
	char	   *shared_ptr;
	ComboCidKey keydata;
	int			num_elements;
	dsm_handle	handle;

	Assert(Gp_role == GP_ROLE_EXECUTE);
	Assert(!Gp_is_writer);

	if (lockHolderProcPtr == NULL)
	{
		/* get lockholder! */
		elog(ERROR, "loadSharedComboCommandId: NO LOCK HOLDER POINTER.");
	}

	/*
	 * Attach to the DSM segment shared by the QE writer process.
	 *
	 * It's possible that the QE write process destroys and reallocates
	 * the array just when we're about to attach to it. Cope with that by
	 * retrying if dsm_attach() fails.
	 */
	for (;;)
	{
		handle = lockHolderProcPtr->comboCidsHandle;

		attached_comboCids = dsm_attach(handle);
		if (attached_comboCids != NULL)
			break;		/* attached successfully */

		/*
		 * Could not attach. Did the QE writer just reallocate a new array?
		 * If so, retry with the new handle. Other errors are not expected.
		 */
		if (handle == lockHolderProcPtr->comboCidsHandle)
			elog(ERROR, "could not attach to shared combo CIDs array");
	}

	/*
	 * Copy the array into local memory.
	 *
	 * Note: we don't use RestoreComboCIDState(), because we don't care about
	 * loading the hash table, just the array. Furthermore,
	 * RestoreComboCIDState assumes that we're starting from a clean slate,
	 * but we might already have old combocids loaded.
	 */
	shared_ptr = dsm_segment_address(attached_comboCids);
	num_elements = *(int *) shared_ptr;
	keydata = (ComboCidKeyData *) (shared_ptr + sizeof(int));

	/* make sure we read the 'num_elements' first */
	pg_read_barrier();

	if (num_elements > sizeComboCids)
	{
		int			newsize = Max(sizeComboCids * 2, num_elements);

		if (comboCids == NULL)
		{
			comboCids = (ComboCidKeyData *)
				MemoryContextAlloc(TopTransactionContext,
								   sizeof(ComboCidKeyData) * newsize);
		}
		else
			comboCids = (ComboCidKeyData *)
				repalloc(comboCids, sizeof(ComboCidKeyData) * newsize);
		sizeComboCids = newsize;
	}

	memcpy(comboCids, keydata, num_elements * sizeof(ComboCidKeyData));
	usedComboCids = num_elements;

	/*
	 * All done, detach from the array.
	 *
	 * XXX: Or would it be better to stay attached, in case we need to load it
	 * again soon?
	 */
	dsm_detach(attached_comboCids);
	attached_comboCids = NULL;
}

void
AtEOXact_ComboCid_Dsm_Detach(void)
{
	if (shared_comboCids != NULL)
	{
		MyProc->comboCidsHandle = 0;
		dsm_detach(shared_comboCids);
		shared_comboCids = NULL;

		shared_usedComboCids = 0;
		shared_sizeComboCids = 0;
	}

}

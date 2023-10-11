/*-------------------------------------------------------------------------
 *
 * storage.c
 *	  code to create and destroy physical storage for relations
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/catalog/storage.c
 *
 * NOTES
 *	  Some of this code used to be in storage/smgr/smgr.c, and the
 *	  function names still reflect that.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "miscadmin.h"

#include "access/visibilitymap.h"
#include "access/xact.h"
#include "access/xloginsert.h"
#include "access/xlogutils.h"
#include "catalog/storage.h"
#include "catalog/storage_xlog.h"
#include "common/relpath.h"
#include "commands/dbcommands.h"
#include "storage/freespace.h"
#include "storage/ipc.h"
#include "storage/md.h"
#include "storage/proc.h"
#include "storage/smgr.h"
#include "utils/dsa.h"
#include "utils/memutils.h"
#include "utils/rel.h"

/*
 * We keep a list of all relations (represented as RelFileNode values)
 * that have been created or deleted in the current transaction.  When
 * a relation is created, we create the physical file immediately, but
 * remember it so that we can delete the file again if the current
 * transaction is aborted.  Conversely, a deletion request is NOT
 * executed immediately, but is just entered in the list.  When and if
 * the transaction commits, we can delete the physical file.
 *
 * To handle subtransactions, every entry is marked with its transaction
 * nesting level.  At subtransaction commit, we reassign the subtransaction's
 * entries to the parent nesting level.  At subtransaction abort, we can
 * immediately execute the abort-time actions for all entries of the current
 * nesting level.
 *
 * NOTE: the list is kept in TopMemoryContext to be sure it won't disappear
 * unbetimes.  It'd probably be OK to keep it in TopTransactionContext,
 * but I'm being paranoid.
 */

typedef struct PendingRelDelete
{
	RelFileNodePendingDelete relnode;		/* relation that may need to be deleted */
	bool		atCommit;		/* T=delete at commit; F=delete at abort */
	int			nestLevel;		/* xact nesting level of request */
	struct PendingRelDelete *next;	/* linked-list link */
	dsa_pointer shmem_ptr;		/* ptr to shared pending delete list */
} PendingRelDelete;

/*
 * Shared pending delete list node.
 * Doubly linked list provides O(1) remove.
 */
typedef struct PendingDeleteListNode
{
	PendingRelXactDelete xrelnode;
	dsa_pointer next;
	dsa_pointer prev;
}			PendingDeleteListNode;

/* A struct to track pending deletes. Placed in static shared memory area. */
typedef struct PendingDeleteShmemStruct
{
	dsa_pointer pdl_head;		/* ptr to list head of PendingDeleteListNode */
	size_t		pdl_count;		/* count of PendingDeleteListNode nodes */
	char		dsa_mem[FLEXIBLE_ARRAY_MEMBER]; /* a minimal memory area which
												 * can be used for dsa
												 * initialization */
}			PendingDeleteShmemStruct;

/*
 * HTAB entry for pending deletes for the given xid.
 */
typedef struct PendingDeleteHtabNode
{
	TransactionId xid;
	List	   *relnode_list;	/* list of RelFileNodePendingDelete */
}			PendingDeleteHtabNode;


static PendingRelDelete *pendingDeletes = NULL; /* head of linked list */
static dsa_area *pendingDeleteDsa = NULL;	/* ptr to DSA area attached by
											 * current process */
static HTAB *pendingDeleteRedo = NULL;	/* HTAB for storing pending deletes
										 * during redo */

static PendingDeleteShmemStruct * PendingDeleteShmem = NULL;	/* shared pending delete
																 * state  */
/*
 * Calculate size for pending delete shmem.
 * The flexible array member should fit DSA.
 */
Size
PendingDeleteShmemSize(void)
{
	Size		size;

	size = offsetof(PendingDeleteShmemStruct, dsa_mem);
	/* dsa initialized over flexible static dsa_mem */
	size = add_size(size, dsa_minimum_size());

	return size;
}

/*
 * Initialize pending delete shmem struct.
 */
void
PendingDeleteShmemInit(void)
{
	Size		size = PendingDeleteShmemSize();
	bool		found;

	PendingDeleteShmem = (PendingDeleteShmemStruct *)
		ShmemInitStruct("Pending Delete",
						size,
						&found);

	if (!found)
	{
		dsa_area   *dsa = dsa_create_in_place(
											  PendingDeleteShmem->dsa_mem,
											  dsa_minimum_size(),
											  LWTRANCHE_PENDING_DELETE_DSA,
											  NULL
		);

		/*
		 * we can't allocate memory segments inside postmaster, so list will
		 * be initialized at runtime
		 */
		PendingDeleteShmem->pdl_head = InvalidDsaPointer;
		PendingDeleteShmem->pdl_count = 0;
		elog(DEBUG2, "Pending delete shared memory initialized.");

		/*
		 * segments will be released by dsm_postmaster_shutdown(), but keep it
		 * clean anyway
		 */
		on_shmem_exit(dsa_on_shmem_exit_release_in_place, (Datum) PendingDeleteShmem->dsa_mem);

		/*
		 * we don't need dsa ptr here, all future dsa calls will be in
		 * backends
		 */
		dsa_detach(dsa);
	}
}

/*
 * Prepend shared list with new pending delete node.
 * dsa - a ptr to currently attached dsa area
 * cur - ptr to already allocated node
 */
static void
PendingDeleteShmemLinkNode(dsa_area *dsa, dsa_pointer cur)
{
	dsa_pointer head;
	PendingDeleteListNode *cur_node;

	cur_node = (PendingDeleteListNode *) dsa_get_address(dsa, cur);

	LWLockAcquire(PendingDeleteLock, LW_EXCLUSIVE);

	head = PendingDeleteShmem->pdl_head;
	cur_node->next = head;
	cur_node->prev = InvalidDsaPointer;
	if (DsaPointerIsValid(head))
	{
		PendingDeleteListNode *head_node = (PendingDeleteListNode *) dsa_get_address(dsa, head);

		head_node->prev = cur;
	}
	PendingDeleteShmem->pdl_head = cur;
	PendingDeleteShmem->pdl_count++;

	LWLockRelease(PendingDeleteLock);

	elog(DEBUG2, "Pending delete rel added to shmem.");
}

/*
 * Remove pending delete node from shared list
 * dsa - a ptr tu currently attached dsa area
 * cur - ptr to node which is already linked to list
 */
static void
PendingDeleteShmemUnlinkNode(dsa_area *dsa, dsa_pointer cur)
{
	dsa_pointer head;
	PendingDeleteListNode *cur_node;

	cur_node = dsa_get_address(dsa, cur);

	LWLockAcquire(PendingDeleteLock, LW_EXCLUSIVE);

	head = PendingDeleteShmem->pdl_head;

	if (DsaPointerIsValid(cur_node->next))
	{
		PendingDeleteListNode *next_node = dsa_get_address(dsa, cur_node->next);

		next_node->prev = cur_node->prev;
	}

	if (DsaPointerIsValid(cur_node->prev))
	{
		PendingDeleteListNode *prev_node = dsa_get_address(dsa, cur_node->prev);

		prev_node->next = cur_node->next;
	}

	if (cur == head)
		PendingDeleteShmem->pdl_head = cur_node->next;

	PendingDeleteShmem->pdl_count--;

	LWLockRelease(PendingDeleteLock);

	elog(DEBUG2, "Pending delete rel removed from shmem.");
}

/*
 * Attach dsa once per process.
 */
static void
PendingDeleteAttachDsa(void)
{
	MemoryContext oldcxt;

	if (pendingDeleteDsa)
		return;

	/*
	 * Keep the DSA area ptr in TopMemoryContext to avoid excessive
	 * attach/detach at every add/remove
	 */
	oldcxt = MemoryContextSwitchTo(TopMemoryContext);
	pendingDeleteDsa = dsa_attach_in_place(PendingDeleteShmem->dsa_mem, NULL);
	MemoryContextSwitchTo(oldcxt);

	/* pin mappings, so they can survive res owner life end */
	dsa_pin_mapping(pendingDeleteDsa);
	/* disconnect from dsa on shmem exit */
	on_shmem_exit(dsa_on_shmem_exit_release_in_place, (Datum) PendingDeleteShmem->dsa_mem);

	elog(DEBUG3, "Pending delete DSA attached");
}

/*
 * Add pending delete node to shmem.
 * Return dsa ptr of newly created node. This ptr can be used for fast remove.
 */
static dsa_pointer
PendingDeleteShmemAdd(RelFileNodePendingDelete * relnode, TransactionId xid)
{
	dsa_pointer pdl_node_dsa;
	PendingDeleteListNode *pdl_node;

	elog(DEBUG2, "Trying to add pending delete rel %d to shmem (xid: %d).", relnode->node.relNode, xid);

	if (xid == InvalidTransactionId || !IsUnderPostmaster)
		return InvalidDsaPointer;

	PendingDeleteAttachDsa();

	pdl_node_dsa = dsa_allocate(pendingDeleteDsa, sizeof(*pdl_node));
	pdl_node = dsa_get_address(pendingDeleteDsa, pdl_node_dsa);

	memcpy(&pdl_node->xrelnode.relnode, relnode, sizeof(*relnode));
	pdl_node->xrelnode.xid = xid;

	PendingDeleteShmemLinkNode(pendingDeleteDsa, pdl_node_dsa);

	return pdl_node_dsa;
}

/*
 * Fast remove pending delete node from shmem.
 * node_ptr is a ptr to already added node.
 */
static void
PendingDeleteShmemRemove(dsa_pointer node_ptr)
{
	elog(DEBUG2, "Trying to remove pending delete rel from shmem.");

	if (!DsaPointerIsValid(node_ptr) || !pendingDeleteDsa)
		return;

	PendingDeleteShmemUnlinkNode(pendingDeleteDsa, node_ptr);

	dsa_free(pendingDeleteDsa, node_ptr);
}

/*
 * Dump all pending delete nodes to char array.
 * Return NULL if there no nodes.
 */
static PendingRelXactDeleteArray *
PendingDeleteXLogShmemDump(Size *size)
{
	dsa_pointer pdl_node_dsa;
	PendingRelXactDeleteArray *xrelnode_array;

	elog(DEBUG2, "Serializing pending deletes to array.");

	PendingDeleteAttachDsa();

	/*
	 * For now, this function can't be called concurrently, so we can use
	 * LW_EXCLUSIVE, but keep the code strict.
	 */
	LWLockAcquire(PendingDeleteLock, LW_SHARED);

	pdl_node_dsa = PendingDeleteShmem->pdl_head;

	if (!DsaPointerIsValid(pdl_node_dsa))
	{
		LWLockRelease(PendingDeleteLock);
		return NULL;
	}

	*size = sizeof(size_t) + sizeof(PendingRelXactDelete) * PendingDeleteShmem->pdl_count;
	xrelnode_array = palloc(*size);
	xrelnode_array->count = 0;

	while (DsaPointerIsValid(pdl_node_dsa))
	{
		PendingDeleteListNode *pdl_node = dsa_get_address(pendingDeleteDsa, pdl_node_dsa);

		memcpy(&xrelnode_array->array[xrelnode_array->count], &pdl_node->xrelnode, sizeof(pdl_node->xrelnode));
		xrelnode_array->count++;

		pdl_node_dsa = pdl_node->next;
	}

	Assert(xrelnode_array->count == PendingDeleteShmem->pdl_count);

	LWLockRelease(PendingDeleteLock);

	elog(DEBUG2, "Pending deletes serialized. Count: %lu.", xrelnode_array->count);

	return xrelnode_array;
}

/*
 * Insert XLOG_PENDING_DELETE record to XLog.
 */
XLogRecPtr
PendingDeleteXLogInsert(void)
{
	XLogRecPtr	recptr;
	Size		size;
	PendingRelXactDeleteArray *xrelnode_array = PendingDeleteXLogShmemDump(&size);

	if (!xrelnode_array)
		return InvalidXLogRecPtr;

	XLogBeginInsert();
	XLogRegisterData((char *) xrelnode_array, size);
	recptr = XLogInsert(RM_XLOG_ID, XLOG_PENDING_DELETE);
	XLogFlush(recptr);

	elog(DEBUG3, "Pending delete XLog record inserted.");

	pfree(xrelnode_array);

	return recptr;
}

/*
 * Add pending delete node during processing of redo records.
 */
static void
PendingDeleteRedoAdd(PendingRelXactDelete * pd)
{
	PendingDeleteHtabNode *h_node;
	bool		found;
	RelFileNodePendingDelete *relnode;

	elog(DEBUG2, "Trying to add pending delete rel %d during redo (xid: %d).", pd->relnode.node.relNode, pd->xid);

	if (pd->xid == InvalidTransactionId)
		return;

	if (!pendingDeleteRedo)
	{
		HASHCTL		ctl;

		memset(&ctl, 0, sizeof(ctl));

		ctl.keysize = sizeof(TransactionId);
		ctl.entrysize = sizeof(PendingDeleteHtabNode);

		pendingDeleteRedo = hash_create("Pending Delete Data", 0, &ctl, HASH_ELEM);

		elog(DEBUG3, "New hash table initialized for pending delete redo.");
	}

	h_node = (PendingDeleteHtabNode *) hash_search(pendingDeleteRedo, &pd->xid, HASH_ENTER, &found);
	if (!found)
	{
		h_node->xid = pd->xid;
		h_node->relnode_list = NIL;

		elog(DEBUG3, "New list initialized for pending delete redo (xid: %d).", pd->xid);
	}

	relnode = palloc(sizeof(*relnode));
	memcpy(relnode, &pd->relnode, sizeof(*relnode));
	h_node->relnode_list = lappend(h_node->relnode_list, relnode);

	elog(DEBUG2, "Pending delete rel %d added during redo (xid: %d).", pd->relnode.node.relNode, pd->xid);
}

/*
 * Replay XLOG_PENDING_DELETE XLog record.
 * Remember all pending delete nodes for possible drop.
 */
void
PendingDeleteRedoRecord(XLogReaderState *record)
{
	PendingRelXactDeleteArray *xrelnode_array = (PendingRelXactDeleteArray *) XLogRecGetData(record);

	Assert(xrelnode_array->count > 0);
	Assert(XLogRecGetDataLen(record) ==
		   (sizeof(xrelnode_array->count) + sizeof(PendingRelXactDelete) * xrelnode_array->count));

	elog(DEBUG2, "Processing pending delete redo record");

	for (size_t i = 0; i < xrelnode_array->count; i++)
		PendingDeleteRedoAdd(&xrelnode_array->array[i]);
}

/*
 * Remove pending delete nodes from redo list.
 * Remove all nodes at once for the given xact.
 */
void
PendingDeleteRedoRemove(TransactionId xid)
{
	PendingDeleteHtabNode *h_node;

	elog(DEBUG2, "Trying to remove pending delete rels during redo (xid: %d).", xid);

	if (xid == InvalidTransactionId || !pendingDeleteRedo)
		return;

	h_node = (PendingDeleteHtabNode *) hash_search(pendingDeleteRedo, &xid, HASH_REMOVE, NULL);

	if (!h_node)
		return;

	/*
	 * Free the whole list. Rels for the given xid are not pending anymore.
	 */
	list_free_deep(h_node->relnode_list);

	elog(DEBUG2, "Pending delete rels removed during redo (xid: %d).", xid);
}

/*
 * Drop all orphaned pending delete nodes.
 */
void
PendingDeleteRedoDropFiles(void)
{
	HASH_SEQ_STATUS seq_status;
	PendingDeleteHtabNode *h_node;

	elog(DEBUG2, "Trying to drop pending delete rels.");

	if (!pendingDeleteRedo || hash_get_num_entries(pendingDeleteRedo) == 0)
		return;

	/* iterate over whole htab */
	hash_seq_init(&seq_status, pendingDeleteRedo);
	while ((h_node = (PendingDeleteHtabNode *) hash_seq_search(&seq_status)) != NULL)
	{
		ListCell   *cell;
		int			i;
		int			count;
		RelFileNodePendingDelete *relnode_array;

		/*
		 * In concurrent environment CREATE and PENDING_DELETE log records
		 * with same rel node may come one after another. This cause
		 * duplicates in pending delete list. To avoid smgr errors, we should
		 * filter it before DropRelationFiles() call.
		 */
		foreach(cell, h_node->relnode_list)
		{
			RelFileNodePendingDelete *o_relnode = (RelFileNodePendingDelete *) lfirst(cell);
			ListCell   *i_cell = lnext(cell);
			ListCell   *i_cell_prev = cell;

			while (i_cell)
			{
				ListCell   *i_cell_next = lnext(i_cell);
				RelFileNodePendingDelete *i_relnode = (RelFileNodePendingDelete *) lfirst(i_cell);

				if (RelFileNodeEquals(o_relnode->node, i_relnode->node))
				{
					elog(DEBUG2, "Duplicate pending delete node found: (rel: %d; xid: %d)",
						 o_relnode->node.relNode, h_node->xid);
					h_node->relnode_list = list_delete_cell(h_node->relnode_list, i_cell, i_cell_prev);
					pfree(i_relnode);
				}
				else
					i_cell_prev = i_cell;

				i_cell = i_cell_next;
			}
		}

		count = list_length(h_node->relnode_list);
		relnode_array = palloc(sizeof(*relnode_array) * count);

		/* copy rels from list to array, we use one batch per one xid to drop */
		foreach_with_count(cell, h_node->relnode_list, i)
		{
			RelFileNodePendingDelete *relnode = (RelFileNodePendingDelete *) lfirst(cell);

			memcpy(&relnode_array[i], relnode, sizeof(*relnode_array));
		}

		DropRelationFiles(relnode_array, count, true);

		elog(DEBUG2, "Pending delete rels were dropped (count: %d; xid: %d).", count, h_node->xid);

		pfree(relnode_array);
		/* we don't need rels list anymore, drop is a final stage */
		list_free_deep(h_node->relnode_list);
	}

	elog(DEBUG2, "Dropping of pending delete rels completed.");

	/* free all the memory, we don't heed htab after recovery */
	hash_destroy(pendingDeleteRedo);
	pendingDeleteRedo = NULL;
}

/*
 * RelationCreateStorage
 *		Create physical storage for a relation.
 *
 * Create the underlying disk file storage for the relation. This only
 * creates the main fork; additional forks are created lazily by the
 * modules that need them.
 *
 * This function is transactional. The creation is WAL-logged, and if the
 * transaction aborts later on, the storage will be destroyed.
 */
SMgrRelation
RelationCreateStorage(RelFileNode rnode, char relpersistence, SMgrImpl smgr_which)
{
	PendingRelDelete *pending;
	SMgrRelation srel;
	BackendId	backend;
	bool		needs_wal;
	TransactionId xid = InvalidTransactionId;
	XLogRecPtr	recptr;

	switch (relpersistence)
	{
		case RELPERSISTENCE_TEMP:
			backend = BackendIdForTempRelations();
			needs_wal = false;
			break;
		case RELPERSISTENCE_UNLOGGED:
			backend = InvalidBackendId;
			needs_wal = false;
			break;
		case RELPERSISTENCE_PERMANENT:
			backend = InvalidBackendId;
			needs_wal = true;
			break;
		default:
			elog(ERROR, "invalid relpersistence: %c", relpersistence);
			return NULL;		/* placate compiler */
	}

	if (needs_wal)
	{
		xid = GetCurrentTransactionId();
		recptr = log_smgrcreate(&rnode, MAIN_FORKNUM, smgr_which);
		
		/*
		 * We should flush last record. Otherwise, in case of a crush after
		 * file creation, file may be orphaned.
		 */
		XLogFlush(recptr);
	}

	srel = smgropen(rnode, backend, smgr_which);
	smgrcreate(srel, MAIN_FORKNUM, false);

	/* Add the relation to the list of stuff to delete at abort */
	pending = (PendingRelDelete *)
		MemoryContextAlloc(TopMemoryContext, sizeof(PendingRelDelete));
	pending->relnode.node = rnode;
	pending->relnode.isTempRelation = backend == TempRelBackendId;
	pending->atCommit = false;	/* delete if abort */
	pending->nestLevel = GetCurrentTransactionNestLevel();
	pending->relnode.smgr_which = smgr_which;
	pending->next = pendingDeletes;
	pending->shmem_ptr = PendingDeleteShmemAdd(&pending->relnode, xid);
	pendingDeletes = pending;

	return srel;
}

/*
 * Perform XLogInsert of an XLOG_SMGR_CREATE record to WAL.
 */
XLogRecPtr
log_smgrcreate(const RelFileNode *rnode, ForkNumber forkNum, SMgrImpl impl)
{
	xl_smgr_create xlrec;
	XLogRecPtr	recptr;

	/*
	 * Make an XLOG entry reporting the file creation.
	 */
	xlrec.rnode = *rnode;
	xlrec.forkNum = forkNum;
	xlrec.impl = impl;

	XLogBeginInsert();
	XLogRegisterData((char *) &xlrec, sizeof(xlrec));
	recptr = XLogInsert(RM_SMGR_ID, XLOG_SMGR_CREATE | XLR_SPECIAL_REL_UPDATE);

	return recptr;
}

/*
 * RelationDropStorage
 *		Schedule unlinking of physical storage at transaction commit.
 */
void
RelationDropStorage(Relation rel)
{
	PendingRelDelete *pending;

	/* Add the relation to the list of stuff to delete at commit */
	pending = (PendingRelDelete *)
		MemoryContextAlloc(TopMemoryContext, sizeof(PendingRelDelete));
	pending->relnode.node = rel->rd_node;
	pending->relnode.isTempRelation = rel->rd_backend == TempRelBackendId;
	pending->atCommit = true;	/* delete if commit */
	pending->nestLevel = GetCurrentTransactionNestLevel();
	pending->relnode.smgr_which =
		RelationIsAppendOptimized(rel) ? SMGR_AO : SMGR_MD;
	pending->next = pendingDeletes;
	/* drop operation can't produce orphaned pending delete node */
	pending->shmem_ptr = InvalidDsaPointer;
	pendingDeletes = pending;

	/*
	 * NOTE: if the relation was created in this transaction, it will now be
	 * present in the pending-delete list twice, once with atCommit true and
	 * once with atCommit false.  Hence, it will be physically deleted at end
	 * of xact in either case (and the other entry will be ignored by
	 * smgrDoPendingDeletes, so no error will occur).  We could instead remove
	 * the existing list entry and delete the physical file immediately, but
	 * for now I'll keep the logic simple.
	 */

	RelationCloseSmgr(rel);
}

/*
 * RelationPreserveStorage
 *		Mark a relation as not to be deleted after all.
 *
 * We need this function because relation mapping changes are committed
 * separately from commit of the whole transaction, so it's still possible
 * for the transaction to abort after the mapping update is done.
 * When a new physical relation is installed in the map, it would be
 * scheduled for delete-on-abort, so we'd delete it, and be in trouble.
 * The relation mapper fixes this by telling us to not delete such relations
 * after all as part of its commit.
 *
 * We also use this to reuse an old build of an index during ALTER TABLE, this
 * time removing the delete-at-commit entry.
 *
 * No-op if the relation is not among those scheduled for deletion.
 */
void
RelationPreserveStorage(RelFileNode rnode, bool atCommit)
{
	PendingRelDelete *pending;
	PendingRelDelete *prev;
	PendingRelDelete *next;

	prev = NULL;
	for (pending = pendingDeletes; pending != NULL; pending = next)
	{
		next = pending->next;
		if (RelFileNodeEquals(rnode, pending->relnode.node)
			&& pending->atCommit == atCommit)
		{
			/* unlink and delete list entry */
			if (prev)
				prev->next = next;
			else
				pendingDeletes = next;
			PendingDeleteShmemRemove(pending->shmem_ptr);
			pfree(pending);
			/* prev does not change */
		}
		else
		{
			/* unrelated entry, don't touch it */
			prev = pending;
		}
	}
}

/*
 * RelationTruncate
 *		Physically truncate a relation to the specified number of blocks.
 *
 * This includes getting rid of any buffers for the blocks that are to be
 * dropped.
 */
void
RelationTruncate(Relation rel, BlockNumber nblocks)
{
	bool		fsm;
	bool		vm;

	/* Open it at the smgr level if not already done */
	RelationOpenSmgr(rel);

	/*
	 * Make sure smgr_targblock etc aren't pointing somewhere past new end
	 */
	rel->rd_smgr->smgr_targblock = InvalidBlockNumber;
	rel->rd_smgr->smgr_fsm_nblocks = InvalidBlockNumber;
	rel->rd_smgr->smgr_vm_nblocks = InvalidBlockNumber;

	/* Truncate the FSM first if it exists */
	fsm = smgrexists(rel->rd_smgr, FSM_FORKNUM);
	if (fsm)
		FreeSpaceMapTruncateRel(rel, nblocks);

	/* Truncate the visibility map too if it exists. */
	vm = smgrexists(rel->rd_smgr, VISIBILITYMAP_FORKNUM);
	if (vm)
		visibilitymap_truncate(rel, nblocks);

	/*
	 * Make sure that a concurrent checkpoint can't complete while truncation
	 * is in progress.
	 *
	 * The truncation operation might drop buffers that the checkpoint
	 * otherwise would have flushed. If it does, then it's essential that
	 * the files actually get truncated on disk before the checkpoint record
	 * is written. Otherwise, if reply begins from that checkpoint, the
	 * to-be-truncated blocks might still exist on disk but have older
	 * contents than expected, which can cause replay to fail. It's OK for
	 * the blocks to not exist on disk at all, but not for them to have the
	 * wrong contents.
	 */
	Assert(!MyProc->delayChkptEnd);
	MyProc->delayChkptEnd = true;

	/*
	 * We WAL-log the truncation before actually truncating, which means
	 * trouble if the truncation fails. If we then crash, the WAL replay
	 * likely isn't going to succeed in the truncation either, and cause a
	 * PANIC. It's tempting to put a critical section here, but that cure
	 * would be worse than the disease. It would turn a usually harmless
	 * failure to truncate, that might spell trouble at WAL replay, into a
	 * certain PANIC.
	 */
	if (RelationNeedsWAL(rel))
	{
		/*
		 * Make an XLOG entry reporting the file truncation.
		 */
		XLogRecPtr	lsn;
		xl_smgr_truncate xlrec;

		xlrec.blkno = nblocks;
		xlrec.rnode = rel->rd_node;
		xlrec.flags = SMGR_TRUNCATE_ALL;

		XLogBeginInsert();
		XLogRegisterData((char *) &xlrec, sizeof(xlrec));

		lsn = XLogInsert(RM_SMGR_ID,
						 XLOG_SMGR_TRUNCATE | XLR_SPECIAL_REL_UPDATE);

		/*
		 * Flush, because otherwise the truncation of the main relation might
		 * hit the disk before the WAL record, and the truncation of the FSM
		 * or visibility map. If we crashed during that window, we'd be left
		 * with a truncated heap, but the FSM or visibility map would still
		 * contain entries for the non-existent heap pages.
		 */
		if (fsm || vm)
			XLogFlush(lsn);
	}

	/*
	 * This will first remove any buffers from the buffer pool that should no
	 * longer exist after truncation is complete, and then truncate the
	 * corresponding files on disk.
	 */
	smgrtruncate(rel->rd_smgr, MAIN_FORKNUM, nblocks);

	/* We've done all the critical work, so checkpoints are OK now. */
	MyProc->delayChkptEnd = false;
}

/*
 * Copy a fork's data, block by block.
 *
 * Note that this requires that there is no dirty data in shared buffers. If
 * it's possible that there are, callers need to flush those using
 * e.g. FlushRelationBuffers(rel).
 */
void
RelationCopyStorage(SMgrRelation src, SMgrRelation dst,
					ForkNumber forkNum, char relpersistence)
{
	PGAlignedBlock buf;
	Page		page;
	bool		use_wal;
	bool		copying_initfork;
	BlockNumber nblocks;
	BlockNumber blkno;

	page = (Page) buf.data;

	/*
	 * The init fork for an unlogged relation in many respects has to be
	 * treated the same as normal relation, changes need to be WAL logged and
	 * it needs to be synced to disk.
	 */
	copying_initfork = relpersistence == RELPERSISTENCE_UNLOGGED &&
		forkNum == INIT_FORKNUM;

	/*
	 * We need to log the copied data in WAL iff WAL archiving/streaming is
	 * enabled AND it's a permanent relation.
	 */
	use_wal = XLogIsNeeded() &&
		(relpersistence == RELPERSISTENCE_PERMANENT || copying_initfork);

	nblocks = smgrnblocks(src, forkNum);

	for (blkno = 0; blkno < nblocks; blkno++)
	{
		/* If we got a cancel signal during the copy of the data, quit */
		CHECK_FOR_INTERRUPTS();

		smgrread(src, forkNum, blkno, buf.data);

		if (!PageIsVerifiedExtended(page, blkno,
									PIV_LOG_WARNING | PIV_REPORT_STAT))
			ereport(ERROR,
					(errcode(ERRCODE_DATA_CORRUPTED),
					 errmsg("invalid page in block %u of relation %s",
							blkno,
							relpathbackend(src->smgr_rnode.node,
										   src->smgr_rnode.backend,
										   forkNum))));

		/*
		 * WAL-log the copied page. Unfortunately we don't know what kind of a
		 * page this is, so we have to log the full page including any unused
		 * space.
		 */
		if (use_wal)
			log_newpage(&dst->smgr_rnode.node, forkNum, blkno, page, false);

		PageSetChecksumInplace(page, blkno);

		/*
		 * Now write the page.  We say isTemp = true even if it's not a temp
		 * rel, because there's no need for smgr to schedule an fsync for this
		 * write; we'll do it ourselves below.
		 */
		smgrextend(dst, forkNum, blkno, buf.data, true);
	}

	/*
	 * If the rel is WAL-logged, must fsync before commit.  We use heap_sync
	 * to ensure that the toast table gets fsync'd too.  (For a temp or
	 * unlogged rel we don't care since the data will be gone after a crash
	 * anyway.)
	 *
	 * It's obvious that we must do this when not WAL-logging the copy. It's
	 * less obvious that we have to do it even if we did WAL-log the copied
	 * pages. The reason is that since we're copying outside shared buffers, a
	 * CHECKPOINT occurring during the copy has no way to flush the previously
	 * written data to disk (indeed it won't know the new rel even exists).  A
	 * crash later on would replay WAL from the checkpoint, therefore it
	 * wouldn't replay our earlier WAL entries. If we do not fsync those pages
	 * here, they might still not be on disk when the crash occurs.
	 */
	if (relpersistence == RELPERSISTENCE_PERMANENT || copying_initfork)
		smgrimmedsync(dst, forkNum);
}

/*
 *	smgrDoPendingDeletes() -- Take care of relation deletes at end of xact.
 *
 * This also runs when aborting a subxact; we want to clean up a failed
 * subxact immediately.
 *
 * Note: It's possible that we're being asked to remove a relation that has
 * no physical storage in any fork. In particular, it's possible that we're
 * cleaning up an old temporary relation for which RemovePgTempFiles has
 * already recovered the physical storage.
 */
void
smgrDoPendingDeletes(bool isCommit)
{
	int			nestLevel = GetCurrentTransactionNestLevel();
	PendingRelDelete *pending;
	PendingRelDelete *prev;
	PendingRelDelete *next;
	int			nrels = 0,
				i = 0,
				maxrels = 0;
	SMgrRelation *srels = NULL;

	prev = NULL;
	for (pending = pendingDeletes; pending != NULL; pending = next)
	{
		next = pending->next;
		if (pending->nestLevel < nestLevel)
		{
			/* outer-level entries should not be processed yet */
			prev = pending;
		}
		else
		{
			/* unlink list entry first, so we don't retry on failure */
			if (prev)
				prev->next = next;
			else
				pendingDeletes = next;
			/* do deletion if called for */
			if (pending->atCommit == isCommit)
			{
				SMgrRelation srel;
				/* GPDB: backend can only be TempRelBackendId or
				 * InvalidBackendId for a given relfile since we don't tie temp
				 * relations to their backends. */
				srel = smgropen(pending->relnode.node,
								pending->relnode.isTempRelation ?
								TempRelBackendId : InvalidBackendId,
								pending->relnode.smgr_which);

				/* allocate the initial array, or extend it, if needed */
				if (maxrels == 0)
				{
					maxrels = 8;
					srels = palloc(sizeof(SMgrRelation) * maxrels);
				}
				else if (maxrels <= nrels)
				{
					maxrels *= 2;
					srels = repalloc(srels, sizeof(SMgrRelation) * maxrels);
				}

				srels[nrels++] = srel;
			}
			/* must explicitly free the list entry */
			PendingDeleteShmemRemove(pending->shmem_ptr);
			pfree(pending);
			/* prev does not change */
		}
	}

	if (nrels > 0)
	{
		smgrdounlinkall(srels, nrels, false);

		for (i = 0; i < nrels; i++)
			smgrclose(srels[i]);

		pfree(srels);
	}
}

/*
 * smgrGetPendingDeletes() -- Get a list of non-temp relations to be deleted.
 *
 * The return value is the number of relations scheduled for termination.
 * *ptr is set to point to a freshly-palloc'd array of RelFileNodes.
 * If there are no relations to be deleted, *ptr is set to NULL.
 *
 * Only non-temporary relations are included in the returned list.  This is OK
 * because the list is used only in contexts where temporary relations don't
 * matter: we're either writing to the two-phase state file (and transactions
 * that have touched temp tables can't be prepared) or we're writing to xlog
 * (and all temporary files will be zapped if we restart anyway, so no need
 * for redo to do it also).
 *
 * Note that the list does not include anything scheduled for termination
 * by upper-level transactions.
 *
 * Greenplum-specific notes: We *do* include temporary relations in the returned
 * list. Because unlike in Upstream Postgres, Greenplum two-phase commits can
 * involve temporary tables, which necessitates including the temporary
 * relations in the two-phase state files (PREPARE xlog record). Otherwise the
 * relation files won't get unlink(2)'d, or the shared buffers won't be
 * dropped at the end of COMMIT phase.
 */
int
smgrGetPendingDeletes(bool forCommit, RelFileNodePendingDelete **ptr)
{
	int			nestLevel = GetCurrentTransactionNestLevel();
	int			nrels;
	RelFileNodePendingDelete *rptr;
	PendingRelDelete *pending;

	nrels = 0;
	for (pending = pendingDeletes; pending != NULL; pending = pending->next)
	{
		if (pending->nestLevel >= nestLevel && pending->atCommit == forCommit
			/*
			 * Greenplum allows transactions that access temporary tables to be
			 * prepared.
			 */
			/* && pending->relnode.backend == InvalidBackendId) */
				)
			nrels++;
	}
	if (nrels == 0)
	{
		*ptr = NULL;
		return 0;
	}
	rptr = (RelFileNodePendingDelete *) palloc(nrels * sizeof(RelFileNodePendingDelete));
	*ptr = rptr;
	for (pending = pendingDeletes; pending != NULL; pending = pending->next)
	{
		if (pending->nestLevel >= nestLevel && pending->atCommit == forCommit
			/*
			 * Keep this loop condition identical to above
			 */
			/* && pending->relnode.backend == InvalidBackendId) */
				)
		{
			*rptr = pending->relnode;
			rptr++;
		}
	}
	return nrels;
}
/*
 *	PostPrepare_smgr -- Clean up after a successful PREPARE
 *
 * What we have to do here is throw away the in-memory state about pending
 * relation deletes.  It's all been recorded in the 2PC state file and
 * it's no longer smgr's job to worry about it.
 */
void
PostPrepare_smgr(void)
{
	PendingRelDelete *pending;
	PendingRelDelete *next;

	for (pending = pendingDeletes; pending != NULL; pending = next)
	{
		next = pending->next;
		pendingDeletes = next;
		/* must explicitly free the list entry */
		PendingDeleteShmemRemove(pending->shmem_ptr);
		pfree(pending);
	}
}

/*
 * AtSubCommit_smgr() --- Take care of subtransaction commit.
 *
 * Reassign all items in the pending-deletes list to the parent transaction.
 */
void
AtSubCommit_smgr(void)
{
	int			nestLevel = GetCurrentTransactionNestLevel();
	PendingRelDelete *pending;

	for (pending = pendingDeletes; pending != NULL; pending = pending->next)
	{
		if (pending->nestLevel >= nestLevel)
			pending->nestLevel = nestLevel - 1;
	}
}

/*
 * AtSubAbort_smgr() --- Take care of subtransaction abort.
 *
 * Delete created relations and forget about deleted relations.
 * We can execute these operations immediately because we know this
 * subtransaction will not commit.
 */
void
AtSubAbort_smgr(void)
{
	smgrDoPendingDeletes(false);
}

void
smgr_redo(XLogReaderState *record)
{
	XLogRecPtr	lsn = record->EndRecPtr;
	uint8		info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

	/* Backup blocks are not used in smgr records */
	Assert(!XLogRecHasAnyBlockRefs(record));

	if (info == XLOG_SMGR_CREATE)
	{
		xl_smgr_create *xlrec = (xl_smgr_create *) XLogRecGetData(record);
		SMgrRelation reln;
		PendingRelXactDelete pd = {{xlrec->rnode, xlrec->impl, false}, XLogRecGetXid(record)};

		reln = smgropen(xlrec->rnode, InvalidBackendId, xlrec->impl);
		smgrcreate(reln, xlrec->forkNum, true);
		PendingDeleteRedoAdd(&pd);
	}
	else if (info == XLOG_SMGR_TRUNCATE)
	{
		xl_smgr_truncate *xlrec = (xl_smgr_truncate *) XLogRecGetData(record);
		SMgrRelation reln;
		Relation	rel;

		/*
		 * AO-specific implementation of SMGR is not needed because truncate
		 * for AO takes a different code path, it does not involve emitting
		 * SMGR_TRUNCATE WAL record.
		 */
		reln = smgropen(xlrec->rnode, InvalidBackendId, SMGR_MD);

		/*
		 * Forcibly create relation if it doesn't exist (which suggests that
		 * it was dropped somewhere later in the WAL sequence).  As in
		 * XLogReadBufferForRedo, we prefer to recreate the rel and replay the
		 * log as best we can until the drop is seen.
		 */
		smgrcreate(reln, MAIN_FORKNUM, true);

		/*
		 * Before we perform the truncation, update minimum recovery point to
		 * cover this WAL record. Once the relation is truncated, there's no
		 * going back. The buffer manager enforces the WAL-first rule for
		 * normal updates to relation files, so that the minimum recovery
		 * point is always updated before the corresponding change in the data
		 * file is flushed to disk. We have to do the same manually here.
		 *
		 * Doing this before the truncation means that if the truncation fails
		 * for some reason, you cannot start up the system even after restart,
		 * until you fix the underlying situation so that the truncation will
		 * succeed. Alternatively, we could update the minimum recovery point
		 * after truncation, but that would leave a small window where the
		 * WAL-first rule could be violated.
		 */
		XLogFlush(lsn);

		if ((xlrec->flags & SMGR_TRUNCATE_HEAP) != 0)
		{
			smgrtruncate(reln, MAIN_FORKNUM, xlrec->blkno);

			/* Also tell xlogutils.c about it */
			XLogTruncateRelation(xlrec->rnode, MAIN_FORKNUM, xlrec->blkno);
		}

		/* Truncate FSM and VM too */
		rel = CreateFakeRelcacheEntry(xlrec->rnode);

		if ((xlrec->flags & SMGR_TRUNCATE_FSM) != 0 &&
			smgrexists(reln, FSM_FORKNUM))
			FreeSpaceMapTruncateRel(rel, xlrec->blkno);
		if ((xlrec->flags & SMGR_TRUNCATE_VM) != 0 &&
			smgrexists(reln, VISIBILITYMAP_FORKNUM))
			visibilitymap_truncate(rel, xlrec->blkno);

		FreeFakeRelcacheEntry(rel);
	}
	else
		elog(PANIC, "smgr_redo: unknown op code %u", info);
}

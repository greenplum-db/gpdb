#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "utils/memutils.h"

#include "../storage_pending.c"

RelFileNodePendingDelete relnode1 = {{1}};
RelFileNodePendingDelete relnode2 = {{2}};
RelFileNodePendingDelete relnode3 = {{3}};
static PendingDeleteListNode pdl_node1;
static PendingDeleteListNode pdl_node2;
static PendingDeleteListNode pdl_node3;
static dsa_pointer dsa_ptr1 = 1;
static dsa_pointer dsa_ptr2 = 2;
static dsa_pointer dsa_ptr3 = 3;

static void
setup_globals(void **state)
{
	/*
	 * Make dsa able to work. Mark that current process is under postmaster.
	 * Write something to pending delete area to pass "is attached" check. We
	 * don't test dsa, so any pointer is OK.
	 */
	IsUnderPostmaster = true;
	pendingDeleteDsa = (dsa_area *) 1;

	MemoryContextInit();
	PendingDeleteShmem = palloc0(sizeof(PendingDeleteShmemStruct));
}

static void
teardown_globals(void **state)
{

}

/*
 * Helper. Convert dsa pointer to real area.
 * Here we don't test dsa and use virtual dsa pointers.
 * This is a function which converts virtual pointer to real node.
 */
static PendingDeleteListNode * dsa_ptr_to_node(dsa_pointer dsa_ptr)
{
	if (dsa_ptr == dsa_ptr1)
		return &pdl_node1;
	else if (dsa_ptr == dsa_ptr2)
		return &pdl_node2;
	else if (dsa_ptr == dsa_ptr3)
		return &pdl_node3;

	return NULL;
}

/*
 * A wrapper for dsa_get_address() function from dsa.h.
 * For our needs pointer to node returned from dsa_ptr_to_node() above.
 */
extern void *__wrap_dsa_get_address(dsa_area *area, dsa_pointer dp);
extern void *__real_dsa_get_address(dsa_area *area, dsa_pointer dp);
void *
__wrap_dsa_get_address(dsa_area *area, dsa_pointer dp)
{
	will_return(dsa_get_address, dsa_ptr_to_node(dp));
	expect_any(dsa_get_address, area);
	expect_any(dsa_get_address, dp);
	return __real_dsa_get_address(area, dp);
}

/* Helper. Add node and check new node is equal. */
static void
add_node(RelFileNodePendingDelete * relnode, TransactionId xid, dsa_pointer dsa_ptr)
{
	PendingDeleteListNode *pdl_node = dsa_ptr_to_node(dsa_ptr);

	will_return(dsa_allocate_extended, dsa_ptr);
	expect_any(dsa_allocate_extended, area);
	expect_any(dsa_allocate_extended, size);
	expect_any(dsa_allocate_extended, flags);

	will_be_called(LWLockAcquire);
	will_be_called(LWLockRelease);
	expect_any(LWLockAcquire, lock);
	expect_any(LWLockAcquire, mode);
	expect_any(LWLockRelease, lock);

	PdlShmemAdd(relnode, xid);

	assert_true(pdl_node->xrelnode.xid == xid);
	assert_memory_equal(&pdl_node->xrelnode.relnode, relnode, sizeof(*relnode));
}

/* Helper. Remove node. */
static void
remove_node(dsa_pointer dsa_ptr)
{
	will_be_called(LWLockAcquire);
	will_be_called(LWLockRelease);
	expect_any(LWLockAcquire, lock);
	expect_any(LWLockAcquire, mode);
	expect_any(LWLockRelease, lock);

	will_be_called(dsa_free);
	expect_any(dsa_free, area);
	expect_any(dsa_free, dp);

	PdlShmemRemove(dsa_ptr);
}

/* Test that all linked list pointers are properly filled during add. */
static void
test__add_node_to_shmem(void **state)
{
	assert_true(PendingDeleteShmem->pdl_head == InvalidDsaPointer);
	assert_int_equal(PendingDeleteShmem->pdl_count, 0);

	add_node(&relnode1, 1, dsa_ptr1);
	assert_true(pdl_node1.next == InvalidDsaPointer);
	assert_true(pdl_node1.prev == InvalidDsaPointer);
	assert_true(PendingDeleteShmem->pdl_head == dsa_ptr1);
	assert_int_equal(PendingDeleteShmem->pdl_count, 1);

	add_node(&relnode2, 2, dsa_ptr2);
	assert_true(pdl_node1.next == InvalidDsaPointer);
	assert_true(pdl_node1.prev == dsa_ptr2);
	assert_true(pdl_node2.next == dsa_ptr1);
	assert_true(pdl_node2.prev == InvalidDsaPointer);
	assert_true(PendingDeleteShmem->pdl_head == dsa_ptr2);
	assert_int_equal(PendingDeleteShmem->pdl_count, 2);

	add_node(&relnode3, 3, dsa_ptr3);
	assert_true(pdl_node1.next == InvalidDsaPointer);
	assert_true(pdl_node1.prev == dsa_ptr2);
	assert_true(pdl_node2.next == dsa_ptr1);
	assert_true(pdl_node2.prev == dsa_ptr3);
	assert_true(pdl_node3.next == dsa_ptr2);
	assert_true(pdl_node3.prev == InvalidDsaPointer);
	assert_true(PendingDeleteShmem->pdl_head == dsa_ptr3);
	assert_int_equal(PendingDeleteShmem->pdl_count, 3);
}

/* Test that all linked list pointers are properly filled during remove. */
static void
test__remove_node_from_shmem(void **state)
{
	assert_true(PendingDeleteShmem->pdl_head == dsa_ptr3);
	assert_int_equal(PendingDeleteShmem->pdl_count, 3);

	remove_node(dsa_ptr2);
	assert_true(pdl_node1.next == InvalidDsaPointer);
	assert_true(pdl_node1.prev == dsa_ptr3);
	assert_true(pdl_node3.next == dsa_ptr1);
	assert_true(pdl_node3.prev == InvalidDsaPointer);
	assert_true(PendingDeleteShmem->pdl_head == dsa_ptr3);
	assert_int_equal(PendingDeleteShmem->pdl_count, 2);

	remove_node(dsa_ptr1);
	assert_true(pdl_node3.next == InvalidDsaPointer);
	assert_true(pdl_node3.prev == InvalidDsaPointer);
	assert_true(PendingDeleteShmem->pdl_head == dsa_ptr3);
	assert_int_equal(PendingDeleteShmem->pdl_count, 1);

	remove_node(dsa_ptr3);
	assert_true(PendingDeleteShmem->pdl_head == InvalidDsaPointer);
	assert_int_equal(PendingDeleteShmem->pdl_count, 0);
}

/* Test correctness of nodes dump. Check their equality with original nodes. */
static void
test__dump_shmem_to_array(void **state)
{
	Size		size;
	PendingRelXactDeleteArray *xrelnode_array;

	will_be_called(LWLockAcquire);
	will_be_called(LWLockRelease);
	expect_any(LWLockAcquire, lock);
	expect_any(LWLockAcquire, mode);
	expect_any(LWLockRelease, lock);

	xrelnode_array = PdlXLogShmemDump(&size);

	assert_int_equal(xrelnode_array->count, 3);

	assert_memory_equal(&xrelnode_array->array[0], &pdl_node3.xrelnode, sizeof(pdl_node3.xrelnode));
	assert_memory_equal(&xrelnode_array->array[1], &pdl_node2.xrelnode, sizeof(pdl_node2.xrelnode));
	assert_memory_equal(&xrelnode_array->array[2], &pdl_node1.xrelnode, sizeof(pdl_node1.xrelnode));
}

/* Check that empty list dumps correctly. */
static void
test__dump_empty_shmem_to_array(void **state)
{
	Size		size;
	PendingRelXactDeleteArray *xrelnode_array;

	will_be_called(LWLockAcquire);
	will_be_called(LWLockRelease);
	expect_any(LWLockAcquire, lock);
	expect_any(LWLockAcquire, mode);
	expect_any(LWLockRelease, lock);

	xrelnode_array = PdlXLogShmemDump(&size);

	assert_true(xrelnode_array == NULL);
}

/*
 * Check correctness of nodes dumping during redo.
 * Check adding, removing and deduplication.
 */
static void
test__redo_prepare_nodes(void **state)
{
	PendingRelXactDelete pd1 = {relnode1, 1};
	PendingRelXactDelete pd2 = {relnode2, 2};
	PendingRelXactDelete pd3 = {relnode3, 3};
	PendingDeleteHtabNode *h_node;
	RelFileNodePendingDelete *relnode_array;
	int			count;

	PdlRedoAdd(&pd1);
	PdlRedoAdd(&pd1);
	PdlRedoAdd(&pd2);
	PdlRedoAdd(&pd3);

	assert_true(pendingDeleteRedo != NULL);

	PdlRedoRemove(3);

	assert_true(pendingDeleteRedo != NULL);

	h_node = (PendingDeleteHtabNode *) hash_search(pendingDeleteRedo, &pd1.xid, HASH_FIND, NULL);
	assert_true(h_node != NULL);
	relnode_array = PdlRedoPrepareDropNodes(h_node, &count);
	assert_int_equal(count, 1);
	assert_memory_equal(&pd1.relnode, &relnode_array[0].node, sizeof(pd1.relnode));

	h_node = (PendingDeleteHtabNode *) hash_search(pendingDeleteRedo, &pd2.xid, HASH_FIND, NULL);
	assert_true(h_node != NULL);
	relnode_array = PdlRedoPrepareDropNodes(h_node, &count);
	assert_int_equal(count, 1);
	assert_memory_equal(&pd2.relnode, &relnode_array[0].node, sizeof(pd2.relnode));

	h_node = (PendingDeleteHtabNode *) hash_search(pendingDeleteRedo, &pd3.xid, HASH_FIND, NULL);
	assert_true(h_node == NULL);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test_setup(setup_globals, setup_globals),
		unit_test(test__add_node_to_shmem),
		unit_test(test__dump_shmem_to_array),
		unit_test(test__remove_node_from_shmem),
		unit_test(test__dump_empty_shmem_to_array),
		unit_test(test__redo_prepare_nodes),
		unit_test_teardown(teardown_globals, teardown_globals),
	};

	return run_tests(tests);
}

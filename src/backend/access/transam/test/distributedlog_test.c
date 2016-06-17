#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"

/* include this here to disable MIRRORED macros */
#include "cdb/cdbpersistentstore.h"

#undef MIRRORED_LOCK_DECLARE
#define MIRRORED_LOCK_DECLARE

#undef MIRRORED_LOCK
#define MIRRORED_LOCK

#undef MIRRORED_UNLOCK
#define MIRRORED_UNLOCK

/* Also ignore elog */
#include "utils/elog.h"

#undef elog
#define elog

/* Actual function body */
#include "../distributedlog.c"

#define DtxLogStartupNumPage 2
#define PageEntryToTransactionId(page, entry) \
	((page) * (TransactionId) ENTRIES_PER_PAGE + (TransactionId) (entry))

/*
 * A bug found in MPP-20426 was we were overrunnig to the next page
 * of DistributedLog.  The intention of the memset with zeors is to
 * reset the reset of the current page if we are in the middle of page,
 * so that we won't see uncommited data due to some recovery work.
 * However, we were doing the wrong math that calculates the size of
 * rest of page as the size of the part preceding to the current xid.
 * The worst scenario was for the subtransaction shared memory, which
 * follows distributed log shared memory to be overwritten.
 */
static MPP_20426(void **state, TransactionId nextXid)
{
	char			pages[BLCKSZ * DtxLogStartupNumPage];
	char			zeros[BLCKSZ];
	int				bytes;

	/* Setup DistributedLogCtl */
	DistributedLogCtl->shared = (SlruShared) malloc(sizeof(SlruSharedData));
	DistributedLogCtl->shared->page_buffer =
			(char **) malloc(DtxLogStartupNumPage * sizeof(char *));
	DistributedLogCtl->shared->page_dirty =
			(bool *) malloc(DtxLogStartupNumPage * sizeof(bool));
	DistributedLogCtl->shared->page_buffer[0] = &pages[0];
	DistributedLogCtl->shared->page_buffer[1] = &pages[BLCKSZ];
	memset(pages, 0x7f, sizeof(pages));
	memset(zeros, 0, sizeof(zeros));

	expect_value(LWLockAcquire, lockid, DistributedLogControlLock);
	expect_value(LWLockAcquire, mode, LW_EXCLUSIVE);
	will_be_called(LWLockAcquire);

	/* This test is only for the case xid is not on the boundary. */
	expect_value(SimpleLruReadPage, ctl, DistributedLogCtl);
	expect_any(SimpleLruReadPage, pageno);
	expect_any(SimpleLruReadPage, write_ok);
	expect_value(SimpleLruReadPage, xid, nextXid);
	will_return(SimpleLruReadPage, 0);

	expect_value(LWLockRelease, lockid, DistributedLogControlLock);
	will_be_called(LWLockRelease);

	/* Run the function. */
	DistributedLog_Startup(nextXid, nextXid);

	/* DistributedLog_Startup should not overwrite the subsequent block. */
	assert_true(pages[BLCKSZ] == 0x7f);

	/* Make sure the part following the xid is zeroed. */
	bytes = TransactionIdToEntry(nextXid) * sizeof(DistributedLogEntry);
	assert_memory_equal(&pages[bytes], zeros, BLCKSZ - bytes);

	free(DistributedLogCtl->shared->page_dirty);
	free(DistributedLogCtl->shared->page_buffer);
	free(DistributedLogCtl->shared);
}

void
test_DistributedLog_Startup_MPP_20426(void **state)
{
	MPP_20426(state, 115064091); /* from observed issue */
	MPP_20426(state, PageEntryToTransactionId(1, 10));
	MPP_20426(state, PageEntryToTransactionId(0x100, ENTRIES_PER_PAGE - 1));
	MPP_20426(state, PageEntryToTransactionId(0x100, 1));
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_DistributedLog_Startup_MPP_20426)
	};
	return run_tests(tests);
}

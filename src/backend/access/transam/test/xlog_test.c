#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../xlog.c"

static void
KeepLogSeg_wrapper(XLogRecPtr recptr, XLogSegNo *logSegNo)
{
	KeepLogSeg(recptr, logSegNo, InvalidXLogRecPtr, false);
}

static void
test_KeepLogSeg(void **state)
{
	XLogRecPtr recptr;
	XLogSegNo  _logSegNo;
	XLogSegNo  PriorRedoPtr;
	bool       _keep_old_wals = false;
	XLogCtlData xlogctl;

	xlogctl.replicationSlotMinLSN = InvalidXLogRecPtr;
	SpinLockInit(&xlogctl.info_lck);
	XLogCtl = &xlogctl;

	/*
	 * 64 segments per Xlog logical file.
	 * Configuring (3, 2), 3 log files and 2 segments to keep (3*64 + 2).
	 */
	wal_keep_segments = 194;

	/*
	 * Set wal segment size to 64 mb
	 */
	wal_segment_size = 64 * 1024 * 1024;

	/************************************************
	 * Current Delete greater than what keep wants,
	 * so, delete offset should get updated
	 ***********************************************/
	/* Current Delete pointer */
	_logSegNo = 3 * XLogSegmentsPerXLogId(wal_segment_size) + 10;

	/*
	 * Current xlog location (4, 1)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 4) << 32 | (wal_segment_size * 1);

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, 63);
	/************************************************/


	/************************************************
	 * Current Delete smaller than what keep wants,
	 * so, delete offset should NOT get updated
	 ***********************************************/
	/* Current Delete pointer */
	_logSegNo = 60;

	/*
	 * Current xlog location (4, 1)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 4) << 32 | (wal_segment_size * 1);

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, 60);
	/************************************************/


	/************************************************
	 * Current Delete smaller than what keep wants,
	 * so, delete offset should NOT get updated
	 ***********************************************/
	/* Current Delete pointer */
	_logSegNo = 1 * XLogSegmentsPerXLogId(wal_segment_size) + 60;

	/*
	 * Current xlog location (5, 8)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 5) << 32 | (wal_segment_size * 8);

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, 1 * XLogSegmentsPerXLogId(wal_segment_size) + 60);
	/************************************************/

	/************************************************
	 * UnderFlow case, curent is lower than keep
	 ***********************************************/
	/* Current Delete pointer */
	_logSegNo = 2 * XLogSegmentsPerXLogId(wal_segment_size) + 1;

	/*
	 * Current xlog location (3, 1)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 3) << 32 | (wal_segment_size * 1);

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, 1);
	/************************************************/

	/************************************************
	 * One more simple scenario of updating delete offset
	 ***********************************************/
	/* Current Delete pointer */
	_logSegNo = 2 * XLogSegmentsPerXLogId(wal_segment_size) + 8;

	/*
	 * Current xlog location (5, 8)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 5) << 32 | (wal_segment_size * 8);

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, 2*XLogSegmentsPerXLogId(wal_segment_size) + 6);
	/************************************************/

	/************************************************
	 * Do nothing if wal_keep_segments is not positive
	 ***********************************************/
	wal_keep_segments = 0;
	_logSegNo = 9 * XLogSegmentsPerXLogId(wal_segment_size) + 45;

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, 9*XLogSegmentsPerXLogId(wal_segment_size) + 45);

	wal_keep_segments = -1;

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, 9*XLogSegmentsPerXLogId(wal_segment_size) + 45);
	/************************************************/

	/************************************************
	 * Replication slot is in use, AND
	 * prior checkpoint is not available at startup, AND
	 * so, set keep to replication slot min LSN.
	 * Current Delete greater than what keep wants,
	 * so, delete offset should get updated
	 ***********************************************/
	max_replication_slots = 1;
	wal_keep_segments = 0;

	/*
	 * Replication slot min LSN location (2, 5)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	xlogctl.replicationSlotMinLSN = ((uint64) 3) << 32 | (wal_segment_size * 5);

	/*
	 * Current xlog location (4, 1)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 4) << 32 | (wal_segment_size * 1);

	/* Current Delete pointer */
	_logSegNo = 4 * XLogSegmentsPerXLogId(wal_segment_size) + 1;

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, 3*XLogSegmentsPerXLogId(wal_segment_size) + 5);
	/************************************************/

	/************************************************
	 * Replication slot is in use, AND
	 * prior checkpoint is not available at startup,
	 * so, set keep to replication slot min LSN.
	 * Current Delete smaller than what keep wants,
	 * so, delete offset should NOT get updated
	 ***********************************************/
	max_replication_slots = 1;
	wal_keep_segments = 0;

	/*
	 * Replication slot min LSN location (4, 12)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	xlogctl.replicationSlotMinLSN = ((uint64) 4) << 32 | (wal_segment_size * 12);

	/*
	 * Current xlog location (4, 10)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 4) << 32 | (wal_segment_size * 11);

	/* Current Delete pointer */
	_logSegNo = 4 * XLogSegmentsPerXLogId(wal_segment_size) + 11;

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, 4*XLogSegmentsPerXLogId(wal_segment_size) + 11);
	/************************************************/

	/************************************************
	 * Replication slots are in use, AND
	 * Prior checkpoint greater than Replication slots min LSN, AND
	 * CkptRedoAfterPriorMinLSN was not initialized at segment restart, AND
	 * CkptRedoBeforeMinLSN is not populated at segment startup
	 * so, keep remains same as replication slot min LSN.
	 * Current Delete greater than what keep wants,
	 * so, delete offset should get updated, AND
	 * _keep_old_wals should be set to true
	 ***********************************************/
	max_replication_slots = 1;
	wal_keep_segments = 0;
	_keep_old_wals = false;

	/*
	 * Replication slot min LSN location (3, 5)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	xlogctl.replicationSlotMinLSN = ((uint64) 3) << 32 | (wal_segment_size * 5);

	/*
	 * Current xlog location (5, 8)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 5) << 32 | (wal_segment_size * 8);

	/* Current Delete pointer */
	_logSegNo = 5 * XLogSegmentsPerXLogId(wal_segment_size) + 8;

	/*
	 * Prior checkpoint location (4, 10)
	 */
	PriorRedoPtr = ((uint64) 4) << 32 | (wal_segment_size * 10);

	KeepLogSeg(recptr, &_logSegNo, PriorRedoPtr, &_keep_old_wals);
	assert_int_equal(_logSegNo, 3*XLogSegmentsPerXLogId(wal_segment_size) + 5);
	assert_true(_keep_old_wals);
	/************************************************/

	/************************************************
	 * Replication slots are in use, AND
	 * Prior checkpoint greater than Replication slots min LSN, AND
	 * CkptRedoAfterPriorMinLSN was set to PriorRedoPtr (4, 10) in previous test, AND
	 * CkptRedoAfterPriorMinLSN greater than Replication slots min LSN, AND
	 * CkptRedoBeforeMinLSN is still not set
	 * so, keep remains same as replication slot min LSN.
	 * Current Delete greater than what keep wants,
	 * so, delete offset should get updated, AND
	 * _keep_old_wals should be set to true
	 ***********************************************/
	max_replication_slots = 1;
	wal_keep_segments = 0;
	_keep_old_wals = false;

	/*
	 * Replication slot min LSN location (3, 11)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	xlogctl.replicationSlotMinLSN = ((uint64) 3) << 32 | (wal_segment_size * 11);

	/*
	 * Current xlog location (6, 3)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 6) << 32 | (wal_segment_size * 3);

	/* Current Delete pointer */
	_logSegNo = 6 * XLogSegmentsPerXLogId(wal_segment_size) + 3;

	/*
	 * Prior checkpoint location (5, 8)
	 */
	PriorRedoPtr = ((uint64) 5) << 32 | (wal_segment_size * 8);

	KeepLogSeg(recptr, &_logSegNo, PriorRedoPtr, &_keep_old_wals);
	assert_int_equal(_logSegNo, 3*XLogSegmentsPerXLogId(wal_segment_size) + 11);
	assert_true(_keep_old_wals);
	/************************************************/

	/************************************************
	 * Replication slots are in use, AND
	 * Prior checkpoint greater than Replication slots min LSN, AND
	 * CkptRedoAfterPriorMinLSN was set to PriorRedoPtr (4, 10) in previous test, AND
	 * CkptRedoAfterPriorMinLSN smaller than Replication slots min LSN, AND
	 * CkptRedoBeforeMinLSN is still not set
	 * so, set CkptRedoBeforeMinLSN and keep to CkptRedoAfterPriorMinLSN, AND
	 * update CkptRedoAfterPriorMinLSN to PriorRedoPtr (5, 10) in current test
	 * Current Delete greater than what keep wants,
	 * so, delete offset should get updated, AND
	 * _keep_old_wals should remain false
	 ***********************************************/
	max_replication_slots = 1;
	wal_keep_segments = 0;
	_keep_old_wals = false;

	/*
	 * Replication slot min LSN location (4, 11)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	xlogctl.replicationSlotMinLSN = ((uint64) 4) << 32 | (wal_segment_size * 11);

	/*
	 * Current xlog location (7, 5)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 7) << 32 | (wal_segment_size * 5);

	/* Current Delete pointer */
	_logSegNo = 7 * XLogSegmentsPerXLogId(wal_segment_size) + 5;

	/*
	 * Prior checkpoint location (6, 3)
	 */
	PriorRedoPtr = ((uint64) 6) << 32 | (wal_segment_size * 3);

	KeepLogSeg(recptr, &_logSegNo, PriorRedoPtr, &_keep_old_wals);
	assert_int_equal(_logSegNo, 4*XLogSegmentsPerXLogId(wal_segment_size) + 10);
	assert_false(_keep_old_wals);
	/************************************************/

	/************************************************
	 * Replication slots are in use, AND
	 * Replication slots min LSN greater than prior checkpoint,
	 * so, set keep to prior checkpoint.
	 * Current Delete greater than what keep wants,
	 * so, delete offset should get updated AND
	 * _keep_old_wals should remain false
	 ***********************************************/
	max_replication_slots = 1;
	wal_keep_segments = 0;
	_keep_old_wals = false;
	/*
	 * Replication slot min LSN location (7, 8)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	xlogctl.replicationSlotMinLSN = ((uint64) 7) << 32 | (wal_segment_size * 8);

	/*
	 * Current xlog location (7, 10)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 7) << 32 | (wal_segment_size * 10);

	/* Current Delete pointer */
	_logSegNo = 7 * XLogSegmentsPerXLogId(wal_segment_size) + 10;

	/*
	 * Prior checkpoint location (7, 5)
	 */
	PriorRedoPtr = ((uint64) 7) << 32 | (wal_segment_size * 5);

	KeepLogSeg(recptr, &_logSegNo, PriorRedoPtr, &_keep_old_wals);
	assert_int_equal(_logSegNo, 7*XLogSegmentsPerXLogId(wal_segment_size) + 5);
	assert_false(_keep_old_wals);
	/************************************************/
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_KeepLogSeg)
	};
	return run_tests(tests);
}

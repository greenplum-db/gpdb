#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../xlog.c"

static void
KeepLogSeg_wrapper(XLogRecPtr recptr, XLogSegNo *logSegNo)
{
	KeepLogSeg(recptr, logSegNo);
}

static void
test_KeepLogSeg(void **state)
{
	XLogRecPtr recptr;
	XLogSegNo  _logSegNo;
	XLogCtlData xlogctl;

	xlogctl.replicationSlotMinLSN = InvalidXLogRecPtr;
	SpinLockInit(&xlogctl.info_lck);
	XLogCtl = &xlogctl;

	/*
	 * 64 segments per Xlog logical file.
	 * Configuring (3, 2), 3 log files and 2 segments to keep (3*64 + 2).
	 */
	wal_keep_segments = 194;

	/************************************************
	 * Current Delete greater than what keep wants,
	 * so, delete offset should get updated
	 ***********************************************/
	/* Current Delete pointer */
	_logSegNo = 3 * XLogSegmentsPerXLogId + 10;

	/*
	 * Current xlog location (4, 1)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 4) << 32 | (XLogSegSize * 1);

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
	recptr = ((uint64) 4) << 32 | (XLogSegSize * 1);

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, 60);
	/************************************************/


	/************************************************
	 * Current Delete smaller than what keep wants,
	 * so, delete offset should NOT get updated
	 ***********************************************/
	/* Current Delete pointer */
	_logSegNo = 1 * XLogSegmentsPerXLogId + 60;

	/*
	 * Current xlog location (5, 8)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 5) << 32 | (XLogSegSize * 8);

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, 1 * XLogSegmentsPerXLogId + 60);
	/************************************************/

	/************************************************
	 * UnderFlow case, curent is lower than keep
	 ***********************************************/
	/* Current Delete pointer */
	_logSegNo = 2 * XLogSegmentsPerXLogId + 1;

	/*
	 * Current xlog location (3, 1)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 3) << 32 | (XLogSegSize * 1);

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, 1);
	/************************************************/

	/************************************************
	 * One more simple scenario of updating delete offset
	 ***********************************************/
	/* Current Delete pointer */
	_logSegNo = 2 * XLogSegmentsPerXLogId + 8;

	/*
	 * Current xlog location (5, 8)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 5) << 32 | (XLogSegSize * 8);

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, 2*XLogSegmentsPerXLogId + 6);
	/************************************************/

	/************************************************
	 * Do nothing if wal_keep_segments is not positive
	 ***********************************************/
	/* Current Delete pointer */
	wal_keep_segments = 0;
	_logSegNo = recptr / XLogSegSize - 3;

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, recptr / XLogSegSize - 3);

	wal_keep_segments = -1;

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, recptr / XLogSegSize - 3);
	/************************************************/
}

static void
test_KeepLogSeg_max_slot_wal_keep_size(void **state)
{
	XLogRecPtr recptr;
	XLogSegNo  _logSegNo;
	XLogCtlData xlogctl;

	xlogctl.replicationSlotMinLSN = ((uint64) 4) << 32 | (XLogSegSize * 0);
	SpinLockInit(&xlogctl.info_lck);
	XLogCtl = &xlogctl;

	/*
	 * 64 segments per Xlog logical file.
	 * Configuring (3, 2), 3 log files and 2 segments to keep (3*64 + 2).
	 */

	wal_keep_segments = 0;

	/************************************************
	 * Current Delete greater than what keep wants,
	 * so, delete offset should get updated.
	 * max_slot_wal_keep_size smaller than the segs
	 * that keeps wants, cut to max_slot_wal_keep_size_mb
	 ***********************************************/
	/* Current Delete pointer */
	_logSegNo = 4 * XLogSegmentsPerXLogId + 20;

	max_slot_wal_keep_size_mb = 5 * 64;

	/*
	 * Current xlog location (4, 10)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 4) << 32 | (XLogSegSize * 10);

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	/* 4 * 64 + 10 - 5 (max_slot_wal_keep_size) */
	assert_int_equal(_logSegNo, 261);
	/************************************************/


	/************************************************
	 * Current Delete greater than what keep wants,
	 * so, delete offset should get updated.
	 * max_slot_wal_keep_size smaller than the segs
	 * that keeps wants, ignore max_slot_wal_keep_size_mb
	 ***********************************************/
	/* Current Delete pointer */
	_logSegNo = 4 * XLogSegmentsPerXLogId + 20;

	max_slot_wal_keep_size_mb = 10 * 64;

	/*
	 * Current xlog location (4, 1)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 4) << 32 | (XLogSegSize * 10);

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	/* cut to the keep (xlogctl.replicationSlotMinLSN) */
	assert_int_equal(_logSegNo, 256);
	/************************************************/

	wal_keep_segments = 15;

	/************************************************
	 * Current Delete greater than what keep wants,
	 * so, delete offset should get updated.
	 * max_slot_wal_keep_size smaller than wal_keep_segments,
	 * max_slot_wal_keep_size doesn't take effect.
	 ***********************************************/
	/* Current Delete pointer */
	_logSegNo = 4 * XLogSegmentsPerXLogId + 20;
	max_slot_wal_keep_size_mb = 5 * 64;

	/*
	 * Current xlog location (4, 1)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 4) << 32 | (XLogSegSize * 10);

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	/* 4 * 64 + 10 - 15 (wal_keep_segments) */
	assert_int_equal(_logSegNo, 251);
	/************************************************/
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_KeepLogSeg),
		unit_test(test_KeepLogSeg_max_slot_wal_keep_size)
	};
	return run_tests(tests);
}

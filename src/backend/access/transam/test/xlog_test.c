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
	wal_keep_size_mb = 194 * 64;

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
	/* Current Delete pointer */
	wal_keep_size_mb = 0;
	_logSegNo = recptr / wal_segment_size - 3;

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, recptr / wal_segment_size - 3);

	wal_keep_size_mb = -1;

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	assert_int_equal(_logSegNo, recptr / wal_segment_size - 3);
	/************************************************/
}

static void
test_KeepLogSeg_max_slot_wal_keep_size(void **state)
{
	XLogRecPtr recptr;
	XLogSegNo  _logSegNo;
	XLogCtlData xlogctl;

	xlogctl.replicationSlotMinLSN = ((uint64) 4) << 32 | (wal_segment_size * 0);
	SpinLockInit(&xlogctl.info_lck);
	XLogCtl = &xlogctl;

	wal_keep_size_mb = 0;

	/************************************************
	 * Current Delete greater than what keep wants,
	 * so, delete offset should get updated.
	 * max_slot_wal_keep_size smaller than the segs
	 * that keeps wants, cut to max_slot_wal_keep_size_mb
	 ***********************************************/
	/* Current Delete pointer */
	_logSegNo = 4 * XLogSegmentsPerXLogId(wal_segment_size) + 20;

	max_slot_wal_keep_size_mb = 5 * 64;

	/*
	 * Current xlog location (4, 10)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 4) << 32 | (wal_segment_size * 10);

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
	_logSegNo = 4 * XLogSegmentsPerXLogId(wal_segment_size) + 20;

	max_slot_wal_keep_size_mb = 10 * 64;

	/*
	 * Current xlog location (4, 1)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 4) << 32 | (wal_segment_size * 10);

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	/* cut to the keep (xlogctl.replicationSlotMinLSN) */
	assert_int_equal(_logSegNo, 256);
	/************************************************/

	wal_keep_size_mb = 15 * (wal_segment_size / (1024 * 1024));

	/************************************************
	 * Current Delete greater than what keep wants,
	 * so, delete offset should get updated.
	 * max_slot_wal_keep_size smaller than wal_keep_size,
	 * max_slot_wal_keep_size doesn't take effect.
	 ***********************************************/
	/* Current Delete pointer */
	_logSegNo = 4 * XLogSegmentsPerXLogId(wal_segment_size) + 20;
	max_slot_wal_keep_size_mb = 5 * 64;

	/*
	 * Current xlog location (4, 1)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr = ((uint64) 4) << 32 | (wal_segment_size * 10);

	KeepLogSeg_wrapper(recptr, &_logSegNo);
	/* 4 * 64 + 10 - 15 (wal_keep_size) */
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

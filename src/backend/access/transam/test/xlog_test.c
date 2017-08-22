#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../xlog.c"

void
test_GetXLogCleanUpToForMaster(void **state)
{
	XLogRecPtr pointer = {0};
	uint32 actual_logId = 0;
	uint32 actual_logSeg = 0;

	GpIdentity.segindex = MASTER_CONTENT_ID;

	will_return(WalSndCtlGetXLogCleanUpTo, &pointer);

	/*
	 * Make the CheckKeepWalSegments return immediately
	 */
	keep_wal_segments = 0;

	GetXLogCleanUpTo(pointer, &actual_logId, &actual_logSeg);
}

#ifdef USE_SEGWALREP
void
test_GetXLogCleanUpToForSegments(void **state)
{
	XLogRecPtr pointer = {0};
	uint32 actual_logId = 0;
	uint32 actual_logSeg = 0;

	GpIdentity.segindex = 0; // not master

	will_return(WalSndCtlGetXLogCleanUpTo, &pointer);

	/*
	 * Make the CheckKeepWalSegments return immediately
	 */
	keep_wal_segments = 0;

	GetXLogCleanUpTo(pointer, &actual_logId, &actual_logSeg);
}
#endif

void
test_CheckKeepWalSegments(void **state)
{
	XLogRecPtr recptr;

	/*
	 * 63 segments per Xlog logical file.
	 * Configuring (3, 2), 3 log files and 2 segments to keep (3*63 + 2).
	 */
	keep_wal_segments = 191;

	/************************************************
	 * Current Delete greater than what keep wants,
	 * so, delete offset should get updated
	 ***********************************************/
	/* Current Delete pointer */
	uint32 _logId = 3;
	uint32 _logSeg = 10;

	/*
	 * Current xlog location (4, 1)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr.xlogid = 4;
	recptr.xrecoff = XLogSegSize * 1;

	CheckKeepWalSegments(recptr, &_logId, &_logSeg);
	assert_int_equal(_logId, 0);
	assert_int_equal(_logSeg, 62);
	/************************************************/


	/************************************************
	 * Current Delete smaller than what keep wants,
	 * so, delete offset should NOT get updated
	 ***********************************************/
	/* Current Delete pointer */
	_logId = 0;
	_logSeg = 60;

	/*
	 * Current xlog location (4, 1)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr.xlogid = 4;
	recptr.xrecoff = XLogSegSize * 1;

	CheckKeepWalSegments(recptr, &_logId, &_logSeg);
	assert_int_equal(_logId, 0);
	assert_int_equal(_logSeg, 60);
	/************************************************/


	/************************************************
	 * Current Delete smaller than what keep wants,
	 * so, delete offset should NOT get updated
	 ***********************************************/
	/* Current Delete pointer */
	_logId = 1;
	_logSeg = 60;

	/*
	 * Current xlog location (5, 8)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr.xlogid = 5;
	recptr.xrecoff = XLogSegSize * 8;

	CheckKeepWalSegments(recptr, &_logId, &_logSeg);
	assert_int_equal(_logId, 1);
	assert_int_equal(_logSeg, 60);
	/************************************************/

	/************************************************
	 * UnderFlow case, curent is lower than keep
	 ***********************************************/
	/* Current Delete pointer */
	_logId = 2;
	_logSeg = 1;

	/*
	 * Current xlog location (3, 1)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr.xlogid = 3;
	recptr.xrecoff = XLogSegSize * 1;

	CheckKeepWalSegments(recptr, &_logId, &_logSeg);
	assert_int_equal(_logId, 0);
	assert_int_equal(_logSeg, 1);
	/************************************************/

	/************************************************
	 * One more simple scenario of upadting delete offset
	 ***********************************************/
	/* Current Delete pointer */
	_logId = 2;
	_logSeg = 8;

	/*
	 * Current xlog location (5, 8)
	 * xrecoff = seg * 67108864 (64 MB segsize)
	 */
	recptr.xlogid = 5;
	recptr.xrecoff = XLogSegSize * 8;

	CheckKeepWalSegments(recptr, &_logId, &_logSeg);
	assert_int_equal(_logId, 2);
	assert_int_equal(_logSeg, 6);
	/************************************************/

	/************************************************
	 * Do nothing if keep_wal_segments is not positive
	 ***********************************************/
	/* Current Delete pointer */
	keep_wal_segments = 0;
	_logId = 9;
	_logSeg = 45;

	CheckKeepWalSegments(recptr, &_logId, &_logSeg);
	assert_int_equal(_logId, 9);
	assert_int_equal(_logSeg, 45);

	keep_wal_segments = -1;

	CheckKeepWalSegments(recptr, &_logId, &_logSeg);
	assert_int_equal(_logId, 9);
	assert_int_equal(_logSeg, 45);
	/************************************************/
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_CheckKeepWalSegments)
		, unit_test(test_GetXLogCleanUpToForMaster)
#ifdef USE_SEGWALREP
		, unit_test(test_GetXLogCleanUpToForSegments)
#endif
	};
	return run_tests(tests);
}

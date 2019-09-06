#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../cdbbufferedread.c"

#include "utils/memutils.h"

static void
test__BufferedReadInit__IsConsistent(void **state)
{
	BufferedRead *bufferedRead = palloc(sizeof(BufferedRead));
	int32 memoryLen = 512; /* maxBufferLen + largeReadLen */
	uint8 *memory = malloc(sizeof(memoryLen));
	char *relname = "test";
	int32 maxBufferLen = 128;
	int32 maxLargeReadLen = 128;

	memset(bufferedRead, 0 , sizeof(BufferedRead));
	/*
	 * Call the function so as to set the above values.
	 */
	BufferedReadInit(bufferedRead, memory, memoryLen, maxBufferLen, maxLargeReadLen, relname);
	/*
	 * Check for consistency
	 */
	assert_int_equal(bufferedRead->maxBufferLen,maxBufferLen);
	assert_int_equal(bufferedRead->maxLargeReadLen,maxLargeReadLen);
	assert_string_equal(bufferedRead->relationName, relname);
	assert_memory_equal(bufferedRead->memory, memory, memoryLen);
	assert_int_equal(bufferedRead->memoryLen, memoryLen);
}

static MemoryContext exception_cxt;

static void
test__BufferedReadUseBeforeBuffer__IsNextReadLenZero(void **state)
{
    BufferedRead *bufferedRead = palloc(sizeof(BufferedRead));
    int32 memoryLen = 512; /* maxBufferLen + largeReadLen */
	uint8 *memory = malloc(sizeof(memoryLen));
	char *relname = "test";
	int32 maxBufferLen = 128;
	int32 maxLargeReadLen = 128;
	int32 nextBufferLen;
	int32 maxReadAheadLen = 64;
        
	memset(bufferedRead, 0 , sizeof(BufferedRead));
	/*
	 * Initialize the buffer
	 */
	BufferedReadInit(bufferedRead, memory, memoryLen, maxBufferLen, maxLargeReadLen, relname);
	/*
	 * filling up the bufferedRead struct
	 */
	bufferedRead->largeReadLen=100;
	bufferedRead->bufferOffset=0;
	bufferedRead->fileLen=200;
	bufferedRead->temporaryLimitFileLen=200;
	bufferedRead->largeReadPosition=50;

	bufferedRead->maxLargeReadLen = 0; /* this will get assigned to nextReadLen(=0) */

    PG_TRY();
	{
    	/*
    	 * This will throw a ereport(ERROR).
    	 */
		BufferedReadUseBeforeBuffer(bufferedRead, maxReadAheadLen, &nextBufferLen);
	}
	PG_CATCH();
	{
		ErrorData *edata;

		MemoryContextSwitchTo(exception_cxt);
		edata = CopyErrorData();
		/*
		 * Validate the expected error
		 */
		assert_true(edata->sqlerrcode == ERRCODE_INTERNAL_ERROR);
		assert_true(edata->elevel == ERROR);
	}
	PG_END_TRY();	
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__BufferedReadUseBeforeBuffer__IsNextReadLenZero),
		unit_test(test__BufferedReadInit__IsConsistent)
	};

	MemoryContextInit();
	exception_cxt = AllocSetContextCreate(TopMemoryContext,
										  "mock error handling context",
										  ALLOCSET_DEFAULT_MINSIZE,
										  ALLOCSET_DEFAULT_INITSIZE,
										  ALLOCSET_DEFAULT_MAXSIZE);

	return run_tests(tests);
}

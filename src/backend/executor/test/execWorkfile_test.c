#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "c.h"
#include "postgres.h"
#include "storage/buffile.h"
#include "storage/bfz.h"
#include "executor/execWorkfile.h"
#include "utils/memutils.h"

/* Ignore elog */
#include "utils/elog.h"

#undef elog
#define elog

/* Provide specialized mock implementations for memory allocation functions */
#undef palloc0
#define palloc0 execWorkfile_palloc0_mock
void *execWorkfile_palloc0_mock(Size size);

#undef pstrdup
#define pstrdup execWorkfile_pstrdup_mock
char *execWorkfile_pstrdup_mock(const char *string);

#include "../execWorkfile.c"

/*
 * This is a mocked version of palloc0 to be used in ExecWorkFile_Create.
 *   It asserts that it is executed in the TopMemoryContext.
 */
void *
execWorkfile_palloc0_mock(Size size)
{
	assert_int_equal(CurrentMemoryContext, TopMemoryContext);
	return MemoryContextAllocZero(CurrentMemoryContext, size);
}

/*
 * This is a mocked version of pstrdup to be used in ExecWorkFile_Create.
 *   It asserts that it is executed in the TopMemoryContext.
 */
char *execWorkfile_pstrdup_mock(const char *string)
{
	assert_int_equal(CurrentMemoryContext, TopMemoryContext);
	return MemoryContextStrdup(CurrentMemoryContext, string);
}


/* ==================== ExecWorkFile_Create ==================== */
/*
 * Test that the ExecWorkFile struct is allocated in TopMemoryContext
 */
void
test__ExecWorkFile_Create__InTopMemContext(void **state)
{
	char *test_filename = "foo";

	will_return(WorkfileQueryspace_AddWorkfile, true);

	expect_value(BufFileCreateFile, fileName, test_filename);
	expect_value(BufFileCreateFile, delOnClose, true);
	expect_value(BufFileCreateFile, interXact, false);
	will_return(BufFileCreateFile, NULL);

	expect_value(BufFileSetWorkfile, buffile, NULL);
	will_be_called(BufFileSetWorkfile);
	/*
	 * Create a new memory context, so that we can distinguish it from
	 * TopMemoryContext.
	 */
	CurrentMemoryContext =
	  AllocSetContextCreate(TopMemoryContext,
				"mock test context",
				ALLOCSET_DEFAULT_MINSIZE,
				ALLOCSET_DEFAULT_INITSIZE,
				ALLOCSET_DEFAULT_MAXSIZE);

	/*
	 * ExecWorkFile_Create will call our mocked palloc0 function execWorkfile__palloc0_mock
	 * and our mocked pstrdup function execWorkfile_pstrdup_mock.
	 * These functions will assert that the allocation of the result happens
	 * in the TopMemoryContext.
	 */
	ExecWorkFile *ewf = ExecWorkFile_Create(test_filename, BUFFILE, true /* delOnClose */, 0 /* compressType */);

}

/* ==================== ExecWorkFile_Open ==================== */
/*
 * Test that the ExecWorkFile struct is allocated in TopMemoryContext
 */
void
test__ExecWorkFile_Open__InTopMemContext(void **state)
{
	char *test_filename = "foo";
	BufFile *bfile = (BufFile *) 0xbffffffe;

	expect_value(BufFileOpenFile, fileName, test_filename);
	expect_value(BufFileOpenFile, create, false);
	expect_value(BufFileOpenFile, delOnClose, false);
	expect_value(BufFileOpenFile, interXact, true);
	will_return(BufFileOpenFile, bfile);

	expect_value(BufFileSetWorkfile, buffile, bfile);
	will_be_called(BufFileSetWorkfile);

	expect_value(BufFileGetSize, buffile, bfile);
	will_return(BufFileGetSize, 0);

	/*
	 * Create a new memory context, so that we can distinguish it from
	 * TopMemoryContext.
	 */
	CurrentMemoryContext =
	  AllocSetContextCreate(TopMemoryContext,
				"mock test context",
				ALLOCSET_DEFAULT_MINSIZE,
				ALLOCSET_DEFAULT_INITSIZE,
				ALLOCSET_DEFAULT_MAXSIZE);

	/*
	 * ExecWorkFile_Open will call our mocked palloc0 function execWorkfile__palloc0_mock
	 * and our mocked pstrdup function execWorkfile_pstrdup_mock.
	 * These functions will assert that the allocation of the result happens
	 * in the TopMemoryContext.
	 */
	ExecWorkFile *ewf = ExecWorkFile_Open(test_filename, BUFFILE, false /* delOnClose */, 0 /* compressType */);

}


/* ==================== main ==================== */
int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__ExecWorkFile_Create__InTopMemContext),
		unit_test(test__ExecWorkFile_Open__InTopMemContext)
	};

	MemoryContextInit();

	return run_tests(tests);
}

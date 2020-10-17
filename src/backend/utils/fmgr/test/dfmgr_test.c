#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "lib/stringinfo.h"
#include "nodes/nodes.h"
#include "utils/memutils.h"

#define ERROR_MESSAGE_MAX_LEN 255

#undef DLSUFFIX
#define DLSUFFIX "To compile dfmgr.c we need to define this"

/* Redefine errdetail and errmsg to capture the error detail for later comparison */
#define errdetail errdetail_internal_impl
#define errdetail_internal errdetail_internal_impl
#define errmsg errmsg_impl
/*
 * Redefine errfinish to throw an error *only if* the error message matches our
 * expectation. Note, an error is the success case
 */
#define errfinish errfinish_impl
/*
 * Mock PG_RE_THROW as well, because we are not using real elog.o.
 * The closest mockery is to call siglongjmp().
 */
#undef PG_RE_THROW
#define PG_RE_THROW() siglongjmp(*PG_exception_stack, 1)

/* Buffer to store the last error mesage from errdetail */
static char lastErrorMsg[ERROR_MESSAGE_MAX_LEN];
/* Buffer to store the expected error message as determined by the test */
static char expectedErrorMsg[ERROR_MESSAGE_MAX_LEN];

static MemoryContext testMemoryContext = NULL;

static void
errfinish_impl(const char *filename, int lineno, const char *funcname)
{
	/* We only throw error if the error message matches our expectation */
	if (0 == strcmp(lastErrorMsg, expectedErrorMsg))
	{
		PG_RE_THROW();
	}
}

static void
errmsg_impl(const char *fmt, ...)
{
}

static void
errdetail_internal_impl(const char* fmt, ...)
{
    StringInfoData	buf;
    initStringInfo(&buf);
    va_list		args;
    va_start(args, fmt);

	appendStringInfoVA(&buf, fmt, args);
	strncpy(lastErrorMsg, buf.data, sizeof(lastErrorMsg) / sizeof(lastErrorMsg[0]));

	pfree(buf.data);
}

#include "../dfmgr.c"

#define EXPECT_EREPORT(LOG_LEVEL)     \
	expect_any(errstart, elevel); \
	expect_any(errstart, domain); \
	if (LOG_LEVEL < ERROR) \
	{ \
    	will_return(errstart, false); \
	} \
    else \
    { \
    	will_return(errstart, true);\
    } \


/*
 * Tests if we error out if the loaded module's expected Pg_magic_struct
 * is smaller (i.e., we have newer fields)
 */
static void
test__incompatible_module_error__struct_size_mismatch(void **state)
{
	Pg_magic_struct module_magic = PG_MODULE_MAGIC_DATA;

	/* Simulate a smaller structure for the module's Pg_magic_struct */
	module_magic.len = offsetof(Pg_magic_struct, version);

	snprintf(expectedErrorMsg, 255, "Magic block has unexpected length or padding difference.");

	EXPECT_EREPORT(ERROR);

	PG_TRY();
	{
		incompatible_module_error("test", &module_magic);
		assert_true(false);
	}
	PG_CATCH();
	{
	}
	PG_END_TRY();
}

/*
 * A utility method that can check header version mismatch error message
 * for different offsets. E.g., mismatch by 1 version, or -1 version tests
 * the error message when module magic's header version is ahead by 1
 * or trailing by 1
 */
static void CheckHeaderVersionMismatch(int diffOffset)
{
	Pg_magic_struct module_magic = PG_MODULE_MAGIC_DATA;

	snprintf(expectedErrorMsg, 255, "Magic block has unexpected length or padding difference.");

	EXPECT_EREPORT(ERROR);

	PG_TRY();
	{
		incompatible_module_error("test", &module_magic);
		assert_true(false);
	}
	PG_CATCH();
	{
	}
	PG_END_TRY();
}

/*
 * Tests if we error out if the loaded module's headerversion is different than ours
 */
static void
test__incompatible_module_error__headerversion_mismatch(void **state)
{
	CheckHeaderVersionMismatch(+1);
	CheckHeaderVersionMismatch(-1);
}

/*
 * Tests if we don't throw a version mismatch error if the header versions are identical
 */
static void
test__incompatible_module_error__headerversion_identical(void **state)
{
	/* Module magic is identical to ours */
	Pg_magic_struct module_magic = PG_MODULE_MAGIC_DATA;

	/* We should expect a "default" error */
	snprintf(expectedErrorMsg, 255, "Magic block has unexpected length or padding difference.");

	EXPECT_EREPORT(ERROR);

	PG_TRY();
	{
		incompatible_module_error("test", &module_magic);
		assert_true(false);
	}
	PG_CATCH();
	{
	}
	PG_END_TRY();
}

/*
 * Sets up the global data structures and the memory context
 */
static void
SetupDataStructures(void **state)
{
	lastErrorMsg[0] = '\0';
	expectedErrorMsg[0] = '\0';

	if (NULL == TopMemoryContext)
	{
		assert_true(NULL == testMemoryContext);
		MemoryContextInit();

		testMemoryContext = AllocSetContextCreate(TopMemoryContext,
        	                      "Test Context",
            	                  ALLOCSET_DEFAULT_MINSIZE,
                	              ALLOCSET_DEFAULT_INITSIZE,
                    	          ALLOCSET_DEFAULT_MAXSIZE);

		MemoryContextSwitchTo(testMemoryContext);
	}

	assert_true(NULL != testMemoryContext &&
			CurrentMemoryContext == testMemoryContext);
}

/*
 * Cleans up memory by reseting testMemoryContext
 */
static void
TeardownDataStructures(void **state)
{
	assert_true(NULL != testMemoryContext &&
			CurrentMemoryContext == testMemoryContext);
	MemoryContextReset(testMemoryContext);
}

int
main(int argc, char* argv[])
{
        cmockery_parse_arguments(argc, argv);

        const UnitTest tests[] = {
        		unit_test_setup_teardown(test__incompatible_module_error__struct_size_mismatch, SetupDataStructures, TeardownDataStructures),
        		unit_test_setup_teardown(test__incompatible_module_error__headerversion_mismatch, SetupDataStructures, TeardownDataStructures),
        		unit_test_setup_teardown(test__incompatible_module_error__headerversion_identical, SetupDataStructures, TeardownDataStructures),
        };
        return run_tests(tests);
}

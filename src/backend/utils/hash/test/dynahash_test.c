#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../dynahash.c"

/* A dummy hash entry structure. */
typedef struct
{
	Oid		key;
	Oid		val;
} OidKeyVal;

/* Once we set this to true, we start erroring out. */
static bool expand_table_start_fail = false;

/*
 * Any value that is power of 2 should be ok, but a smaller size hits
 * the error quickly.
 */
#define TEST_SEGSIZE (16)

/*
 * This special allocation routine will issue an error only if it is told
 * to do so *and* the size argument says it's from alloc_seg.  Though it
 * is not clean, the assumption is HASH_BUCKET is a pointer and we gave
 * hctl->ssize to hash_create.  See alloc_seg in dynahash.
 */
static void *
alloc_for_expand_table_failure(Size size)
{
	if (expand_table_start_fail && size == sizeof(void *) * TEST_SEGSIZE)
		elog(ERROR, "out of memory");
	return malloc(size);
}

/*
 * MPP-18623: if we error out from expand_table, we ended up having an
 * incomplete entry left behind in the hash table, which causes unexpected
 * issue at the cleanup code.  This test is to make sure such failure of
 * hash expansion won't create an incomplete entry.
 */
static void
test__expand_table(void **state)
{
	HASHCTL		info;
	int			hash_flags;
	HTAB	   *htab;
	int			i;
	bool		got_error = false;
	OidKeyVal  *elem;
	HASH_SEQ_STATUS status;

	info.keysize = sizeof(Oid);
	info.entrysize = sizeof(OidKeyVal);
	info.hash = oid_hash;
	info.alloc = alloc_for_expand_table_failure;
	info.ssize = TEST_SEGSIZE;
	hash_flags = (HASH_ELEM | HASH_FUNCTION | HASH_ALLOC | HASH_SEGMENT);

	htab = hash_create("Test hash",
					   128,
					   &info,
					   hash_flags);

	/*
	 * As adding more entries, we'll hit OOM ERROR at the expand_table.
	 * After seeing that error, we'll check if no entry in the hash tble
	 * is in the intermediate state by looking at the val.
	 */
	expand_table_start_fail = true;
	for (i = 0; i < 1024; i++) {
		bool		found;
		Oid			key = (Oid) i;

		PG_TRY();
		{
			elem = (OidKeyVal *) hash_search(htab,
											 (void *) &key,
											 HASH_ENTER, &found);
			assert_false(found);
			/* Mark this entry as complete. */
			elem->val = 1;
		}
		PG_CATCH();
		{
			got_error = true;
		}
		PG_END_TRY();

		if (got_error)
			break;
	}

	/* Expect we have hit OOM */
	assert_true(got_error);

	will_return(GetCurrentTransactionNestLevel, 1);
	hash_seq_init(&status, htab);
	while ((elem = (OidKeyVal *) hash_seq_search(&status)) != NULL)
	{
		/*
		 * If we have created an entry without setting the val correctly,
		 * we are in trouble.
		 */
		assert_int_equal(elem->val, 1);
	}
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__expand_table)
	};

	MemoryContextInit();

	return run_tests(tests);
}

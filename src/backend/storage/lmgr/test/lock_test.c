#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

/* For RemoveLocalLock, we need to check if NULL is not passed to pfree */
#include "postgres.h"
#undef pfree
#define pfree(x) do { \
	assert_true(x != NULL); \
	free(x); \
} while(0)
#include "../lock.c"


/*
 * MPP-18576: RemoveLocalLock should be aware lockOwners can be NULL
 * in case of OOM after populating the hash entry.
 */
static void
test__RemoveLocalLock_Null(void **state)
{
	HASHCTL		info;
	int			hash_flags;

	LOCALLOCKTAG localtag;
	LOCALLOCK  *locallock;
	bool		found;

	info.keysize = sizeof(LOCALLOCKTAG);
	info.entrysize = sizeof(LOCALLOCK);
	info.hash = tag_hash;
	hash_flags = (HASH_ELEM | HASH_FUNCTION);

	LockMethodLocalHash = hash_create("LOCALLOCK hash",
									  128,
									  &info,
									  hash_flags);

	/* Create a dummy local lock */
	MemSet(&localtag, 0, sizeof(localtag));
	localtag.lock.locktag_field1 = 1;
	localtag.lock.locktag_field2 = 2;
	localtag.lock.locktag_field3 = 3;
	localtag.lock.locktag_field4 = 4;
	localtag.mode = AccessShareLock;

	locallock = (LOCALLOCK *) hash_search(LockMethodLocalHash,
										  (void *) &localtag,
										  HASH_ENTER, &found);

	assert_false(found);

	/* Assume lockOwners is NULL and we go into cleanup */
	locallock->lockOwners = NULL;
	RemoveLocalLock(locallock);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__RemoveLocalLock_Null)
	};

	return run_tests(tests);
}

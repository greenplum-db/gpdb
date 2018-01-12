/*-------------------------------------------------------------------------
 *
 * workfile_mgr_test.c
 *	 Unit tests for workfile manager and cache.
 *
 * Portions Copyright (c) 2012, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/test/regress/workfile_mgr_test.c
 *
 *-------------------------------------------------------------------------
 */

#include <unistd.h>

#include "postgres.h"

#include "access/xact.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbvars.h"
#include "executor/execWorkfile.h"
#include "miscadmin.h"
#include "port/atomics.h"
#include "storage/bfz.h"
#include "storage/buffile.h"
#include "utils/builtins.h"
#include "utils/logtape.h"
#include "utils/memutils.h"
#include "utils/sharedcache.h"
#include "utils/syncrefhashtable.h"

#define TEST_NAME_LENGTH 50
#define TEST_HT_NUM_ELEMENTS 8192

/* Number of Workfiles created during the "stress" workfile test */
#define TEST_MAX_NUM_WORKFILES 100000

typedef struct TestSyncHTElt
{
	char key[TEST_NAME_LENGTH];
	int data;
	int numChildren;
	int32 pinCount;
} TestSyncHTElt;

typedef struct TestCacheElt
{
	char key[TEST_NAME_LENGTH];
	int data;
} TestCacheElt;

typedef struct TestPopParam
{
	char key[TEST_NAME_LENGTH];
	int data;
} TestPopParam;

int tests_passed;
int tests_failed;
int tests_total;

/* Test definitions */
static bool syncrefhashtable_test_basics(void);
static bool syncrefhashtable_test_concurrency_non_overlap(void);
static bool syncrefhashtable_test_error_cases(void);
static bool syncrefhashtable_test_full_table(void);

static bool cache_test_acquire(void);
static bool cache_test_insert(void);
static bool cache_test_remove(void);
static bool cache_test_concurrency(void);

static bool bfz_test_reopen(void);
static bool execworkfile_buffile_test(void);
static bool execworkfile_bfz_zlib_test(void);
static bool execworkfile_bfz_uncompressed_test(void);
static bool fd_tests(void);
static bool buffile_size_test(void);
static bool buffile_large_file_test(void);
static bool logicaltape_test(void);
static bool fd_large_file_test(void);
static bool execworkfile_create_one_MB_file(void);
static bool workfile_queryspace(void);
static bool workfile_fill_sharedcache(void);
static bool workfile_create_and_set_cleanup(void);
static bool workfile_create_and_individual_cleanup(void);

static bool atomic_test(void);

/* Local helpers */
static SyncHT *syncrefhastable_test_create(void);
static Cache *cache_test_create();

/* Unit tests helper */
static void unit_test_result(bool result);
static void unit_test_reset(void);
static bool unit_test_summary(void);

extern Datum gp_workfile_mgr_test_harness(PG_FUNCTION_ARGS);
extern Datum gp_workfile_mgr_reset_segspace(PG_FUNCTION_ARGS);

#define GET_STR(textp) DatumGetCString(DirectFunctionCall1(textout, PointerGetDatum(textp)))

typedef bool (*gp_workfile_mgr_test)(void);

typedef struct test_def
{
	char *test_name;
	gp_workfile_mgr_test test_func;
} test_def;

static test_def test_defns[] = {
		{"syncrefhashtable_test_basics", syncrefhashtable_test_basics},
		{"syncrefhashtable_test_concurrency_non_overlap", syncrefhashtable_test_concurrency_non_overlap},
		{"syncrefhashtable_test_error_cases", syncrefhashtable_test_error_cases},
		{"syncrefhashtable_test_full_table", syncrefhashtable_test_full_table},
		{"cache_test_aquire", cache_test_acquire},
		{"cache_test_insert", cache_test_insert},
		{"cache_test_remove", cache_test_remove},
		{"cache_test_concurrency", cache_test_concurrency},
		{"bfz_test_reopen", bfz_test_reopen},
		{"execworkfile_buffile_test", execworkfile_buffile_test},
		{"execworkfile_bfz_zlib_test", execworkfile_bfz_zlib_test},
		{"execworkfile_bfz_uncompressed_test", execworkfile_bfz_uncompressed_test},
		{"atomic_test", atomic_test},
		{"fd_tests", fd_tests},
		{"buffile_size_test", buffile_size_test},
		{"buffile_large_file_test", buffile_large_file_test},
		{"logicaltape_test", logicaltape_test},
		{"fd_large_file_test",fd_large_file_test},
		{"execworkfile_create_one_MB_file",execworkfile_create_one_MB_file},
		{"workfile_queryspace", workfile_queryspace},
		{"workfile_fill_sharedcache", workfile_fill_sharedcache},
		{"workfile_create_and_set_cleanup", workfile_create_and_set_cleanup},
		{"workfile_create_and_individual_cleanup", workfile_create_and_individual_cleanup},
		{NULL, NULL}, /* This has to be the last element of the array */
};

PG_FUNCTION_INFO_V1(gp_workfile_mgr_test_harness);
Datum
gp_workfile_mgr_test_harness(PG_FUNCTION_ARGS)
{
	bool result = true;

	Assert(PG_NARGS() == 1);

	char *test_name = GET_STR(PG_GETARG_TEXT_P(0));
	bool run_all_tests = strcasecmp(test_name, "all") == 0;
	bool ran_any_tests = false;

	int crt_test = 0;
	while (NULL != test_defns[crt_test].test_name)
	{
		if (run_all_tests || (strcasecmp(test_name, test_defns[crt_test].test_name) == 0))
		{
			result = result && test_defns[crt_test].test_func();
			ran_any_tests = true;
		}
		crt_test++;
	}

	if (!ran_any_tests)
	{
		elog(LOG, "No tests match given name: %s", test_name);
	}

	PG_RETURN_BOOL(ran_any_tests && result);
}

/*
 * gp_workfile_mgr_reset_segspace
 *    Function to reset the used segspace on a segment
 *    This directly manipulates the segspace counter and
 *    should be used for testing purposes only
 *  Returns the size before the reset
 */
PG_FUNCTION_INFO_V1(gp_workfile_mgr_reset_segspace);
Datum
gp_workfile_mgr_reset_segspace(PG_FUNCTION_ARGS)
{
	int64 size = WorkfileSegspace_GetSize();
	WorkfileSegspace_Commit(0, size);
	return Int64GetDatum(size);
}

/*
 * Callback function to do the client-side cleanup for an entry that is being
 * removed from the cache.
 *
 * Resource is the payload of the entry.
 */
static void
cacheEltCleanup(const void *resource)
{
	TestCacheElt *elt = (TestCacheElt *) resource;

	CACHE_ASSERT_VALID(CACHE_ENTRY_HEADER(elt));

	elog(gp_workfile_caching_loglevel, "deleting TestCacheElt. [key=""%s"", data=%d]",
			elt->key, elt->data);

	elt->data = -1;
}

static void
cacheEltPopulate(const void *resource, const void *param)
{
	Assert(NULL != resource);

	if (NULL == param)
	{
		elog(gp_workfile_caching_loglevel, "Returning uninitialized entry");
		return;
	}

	TestCacheElt *elt = (TestCacheElt *) resource;
	TestPopParam *eltInfo = (TestPopParam *) param;

	strlcpy(elt->key, eltInfo->key, sizeof(elt->key));
	elt->data = eltInfo->data;
}

/*
 * Function to be called to cleanup in event of client error
 *
 * Releases and surrenders all client owned entries back to the cache
 */
static void
Cache_TeardownCallback(XactEvent event, void *arg)
{
	Assert(NULL != arg);

	Cache *cache = (Cache *) arg;

	elog(LOG, "Calling cleanup now for cache %s with %d used entries",
			cache->cacheName,
			cache->cacheHdr->nEntries - cache->cacheHdr->cacheStats.noFreeEntries);

	Cache_SurrenderClientEntries(cache);
}

static Cache *
cache_test_create()
{
	CacheCtl cacheCtl;
	MemSet(&cacheCtl, 0, sizeof(CacheCtl));

	cacheCtl.entrySize = sizeof(TestCacheElt);
	cacheCtl.keySize = TEST_NAME_LENGTH;
	cacheCtl.keyOffset = GPDB_OFFSET(TestCacheElt, key);

	cacheCtl.hash = string_hash;
	cacheCtl.keyCopy = (HashCopyFunc) strlcpy;
	cacheCtl.match = (HashCompareFunc) strncmp;

	cacheCtl.cleanupEntry = cacheEltCleanup;
	cacheCtl.populateEntry = cacheEltPopulate;

	cacheCtl.maxSize = TEST_HT_NUM_ELEMENTS;
	cacheCtl.cacheName = "Test Cache";

	cacheCtl.baseLWLockId = FirstWorkfileMgrLock;
	cacheCtl.numPartitions = NUM_WORKFILEMGR_PARTITIONS;


	Cache *cache = NULL;
	cache = Cache_Create(&cacheCtl);

	Assert(cache);
	RegisterXactCallbackOnce(Cache_TeardownCallback, cache);

	return cache;
}

static bool
cache_test_acquire(void)
{
	unit_test_reset();

	elog(LOG, "Running test: cache_test_acquire");

	elog(LOG, "Running sub-test: CacheCreate");
	Cache *cache = cache_test_create();
	unit_test_result(cache != NULL);

	elog(LOG, "Running sub-test: CacheAcquireEntry");

	TestPopParam param;
	strlcpy(param.key, "Test Key 1", sizeof(param.key));
	param.data = 4567;

	CacheEntry *entry = Cache_AcquireEntry(cache, &param);
	unit_test_result(entry != NULL);


	elog(LOG, "Running sub-test: CacheSurrenderEntry");
	Cache_Release(cache, entry);
	unit_test_result(true);

	return unit_test_summary();
}

static bool
cache_test_insert()
{
	unit_test_reset();

	elog(LOG, "Running test: cache_test_insert");

	elog(LOG, "Running sub-test: CacheCreate");
	Cache *cache = cache_test_create();
	unit_test_result(cache != NULL);

	elog(LOG, "Running sub-test: CacheAcquireEntry");

	TestPopParam param;
	strlcpy(param.key, "Test Key 2", sizeof(param.key));
	param.data = 1111;

	CacheEntry *entry = Cache_AcquireEntry(cache, &param);
	unit_test_result(entry != NULL);

	elog(LOG, "Running sub-test: CacheInsert");

	Cache_Insert(cache, entry);
	Cache_Release(cache, entry);
	unit_test_result(true);

	/* Look-up test */
	elog(LOG, "Running sub-test: CacheLookup");

	strlcpy(param.key, "Test Key 2", sizeof(param.key));
	param.data = 1111;
	CacheEntry *localEntry = Cache_AcquireEntry(cache, &param);

	Cache_Release(cache, localEntry);
	return unit_test_summary();

}

static bool
cache_test_remove(void)
{
	unit_test_reset();

	elog(LOG, "Running test: cache_test_remove");

	elog(LOG, "Running sub-test: CacheCreate");
	Cache *cache = cache_test_create();
	unit_test_result(cache != NULL);

	char *testKey = "Test Remove Key 2";

	/* Insert one entry */
	TestPopParam param;
	strlcpy(param.key, testKey, sizeof(param.key));
	param.data = 1111;
	CacheEntry *entry1 = Cache_AcquireEntry(cache, &param);
	Cache_Insert(cache, entry1);
	Cache_Release(cache, entry1);

	/* Insert another entry */
	strlcpy(param.key, testKey, sizeof(param.key));
	param.data = 2222;
	CacheEntry *entry2 = Cache_AcquireEntry(cache, &param);
	Cache_Insert(cache, entry2);
	Cache_Release(cache, entry2);

	/* Look-up and remove an entry */
	strlcpy(param.key, testKey, sizeof(param.key));
	param.data = 2222;
	CacheEntry *localEntry = Cache_AcquireEntry(cache, &param);

	Cache_Release(cache, localEntry);

	return unit_test_summary();
}

static bool
cache_test_concurrency(void)
{
	unit_test_reset();

	elog(LOG, "Running test: cache_test_concurrency");

	elog(LOG, "Running sub-test: CacheCreate");
	Cache *cache = cache_test_create();
	unit_test_result(cache != NULL);

	/* Number of elements in the array to hold test entries */
	const int noTestEntries = 1000;
	CacheEntry *entries[noTestEntries];
	char key[TEST_NAME_LENGTH];

	elog(LOG, "Running sub-test: Cache insert/lookup/delete many elements");
	int noTestIterations = 5000;
	int i = 0;
	int iterNo = 0;
	bool testFailed = false;

	TestPopParam param;

	for (iterNo = 0; iterNo < noTestIterations ; iterNo ++)
	{
		if (testFailed)
		{
			break;
		}

		/* Insert noTestEntries elements */
		for (i=0; i < noTestEntries; i++)
		{
			/* If we include Pid in the key, we get short chains */
			/* snprintf(key, TEST_NAME_LENGTH, "PID=%d cache key no. %d", MyProcPid, i); */
			snprintf(key, TEST_NAME_LENGTH, "cache key no. %d", i);

			strlcpy(param.key, key, sizeof(param.key));
			param.data = MyProcPid;

			entries[i] = Cache_AcquireEntry(cache, &param);

			if (entries[i] == NULL)
			{
				elog(LOG, "Could not acquire entry");
				testFailed = true;
				break;
			}

			Cache_Insert(cache, entries[i]);
			Cache_Release(cache, entries[i]);
		}

		if (testFailed)
		{
			break;
		}

		/* Look up noTestEntries */
		for (i=0; i < noTestEntries; i++)
		{
			//snprintf(key, TEST_NAME_LENGTH, "PID=%d cache key no. %d", MyProcPid, i);
			snprintf(key, TEST_NAME_LENGTH, "cache key no. %d", i);
			strlcpy(param.key, key, sizeof(param.key));
			param.data = MyProcPid;

			CacheEntry *localEntry = Cache_AcquireEntry(cache, &param);
			if (localEntry == NULL)
			{
				elog(LOG, "Could not acquire entry");
				testFailed = true;
				break;
			}

			Cache_Release(cache, localEntry);
		}

		if (testFailed)
		{
			break;
		}

		/* Delete and release noTestEntries */
		for (i=0; i < noTestEntries; i++)
		{
			Cache_Remove(cache, entries[i]);
			Cache_Release(cache, entries[i]);
		}

		CHECK_FOR_INTERRUPTS();

		/* Look up no TestEntries again, should not be found */
		for (i=0; i < noTestEntries; i++)
		{
			//snprintf(key, TEST_NAME_LENGTH, "PID=%d cache key no. %d", MyProcPid, i);
			snprintf(key, TEST_NAME_LENGTH, "cache key no. %d", i);
			strlcpy(param.key, key, sizeof(param.key));
			param.data = MyProcPid;

			CacheEntry *localEntry = Cache_AcquireEntry(cache, &param);
			if (localEntry == NULL)
			{
				elog(LOG, "Could not acquire entry");
				testFailed = true;
				break;
			}

			Cache_Release(cache, localEntry);
		}

		CHECK_FOR_INTERRUPTS();
	}

	unit_test_result(!testFailed);
	return unit_test_summary();
}

/*
 * Callback function to test if an entry in the hashtable is "empty"
 */
static bool
isTestEltEmpty(const void *entry)
{
	TestSyncHTElt *testElt = (TestSyncHTElt *) entry;
	return testElt->numChildren == 0;
}

/*
 * Callback function to initialize an entry before returning to the caller
 */
static void
initTestElt(void *entry)
{
	TestSyncHTElt *testElt = (TestSyncHTElt *) entry;
	testElt->numChildren = 0;
	testElt->pinCount = 0;
}

static bool
syncrefhashtable_test_basics(void)
{
	unit_test_reset();

	elog(LOG, "Running test: syncrefhashtable_test_basics");

	elog(LOG, "Running sub-test: SyncHTCreate");
	SyncHT *syncHT = syncrefhastable_test_create();
	unit_test_result(syncHT != NULL);
	Assert(syncHT);

	char *keyText = "Key one";

	elog(LOG, "Running sub-test: SyncHTInsert first element");
	bool existing = false;

	/* Number of elements in the array to hold test entries */
	const int noTestEntries = 20;

	TestSyncHTElt *elements[noTestEntries];
	long numEntries = 0;

	elements[0] = (TestSyncHTElt *) SyncHTInsert(syncHT, (void *) keyText, &existing);
	unit_test_result(elements[0] != NULL && !existing);

	elog(LOG, "Running sub-test: SyncHTInsert duplicate element");
	existing = false;
	elements[1] = (TestSyncHTElt *) SyncHTInsert(syncHT, (void *) keyText, &existing);
	unit_test_result(elements[1] != NULL && existing);

	elog(LOG, "Running sub-test: SyncHTInsert unique element");
	keyText = "Key two";
	existing = false;
	elements[2] = (TestSyncHTElt *) SyncHTInsert(syncHT, (void *) keyText, &existing);
	unit_test_result(elements[2] != NULL && !existing);

	elog(LOG, "Running sub-test: SyncHTRelease");
	bool deleted[noTestEntries];
	deleted[0] = SyncHTRelease(syncHT, elements[0]);
	deleted[1] = SyncHTRelease(syncHT, elements[1]);
	deleted[2] = SyncHTRelease(syncHT, elements[2]);
	numEntries = SyncHTNumEntries(syncHT);

	unit_test_result(numEntries == 0 && !deleted[0] && deleted[1] && deleted[2]);


	elog(LOG, "Running sub-test: SyncHTDestroy");
	SyncHTDestroy(syncHT);
	unit_test_result(true);

	return unit_test_summary();
}

static bool
syncrefhashtable_test_concurrency_non_overlap(void)
{
	unit_test_reset();

	elog(LOG, "Running test: syncrefhashtable_test_concurrency_non_overlap");

	elog(LOG, "Running sub-test: SyncHTCreate");
	SyncHT *syncHT = syncrefhastable_test_create();
	unit_test_result(syncHT != NULL);
	Assert(syncHT);

	/* Number of elements in the array to hold test entries */
	const int noTestEntries = 1000;
	TestSyncHTElt *elements[noTestEntries];
	bool existing = false, deleted = false;

	char key[TEST_NAME_LENGTH];

	elog(LOG, "Running sub-test: SyncHTInsert many elements");
	int noTestIterations = 5000;
	int i = 0;
	int iterNo = 0;
	bool testFailed = false;

	for (iterNo = 0; iterNo < noTestIterations ; iterNo ++)
	{
		if (testFailed)
		{
			break;
		}

		/* Insert noTestEntries elements */
		for (i=0; i < noTestEntries; i++)
		{
			snprintf(key, TEST_NAME_LENGTH, "PID=%d key no. %d", MyProcPid, i);

			elements[i] = SyncHTInsert(syncHT, key, &existing);
			if (existing)
			{
				testFailed = true;
				break;
			}
			elements[i]->numChildren = 123;
		}

		/* Release noTestEntries elements, don't get deleted */
		for (i=0; i < noTestEntries; i++)
		{
			deleted = SyncHTRelease(syncHT, elements[i]);
			if (deleted)
			{
				testFailed = true;
				break;
			}
		}

//		numEntries = SyncHTNumEntries(syncHT); /* XXX This does not hold for concurrency */
//		if (numEntries != noTestEntries)
//		{
//			testFailed = true;
//			break;
//		}


		/* Look-up noTestEntries elements. Empty them and release them, they should get deleted */
		for (i=0; i < noTestEntries; i++)
		{
			snprintf(key, TEST_NAME_LENGTH, "PID=%d key no. %d", MyProcPid, i);
			TestSyncHTElt *foundElt = (TestSyncHTElt *) SyncHTLookup(syncHT, key);
			if (!foundElt)
			{
				testFailed = true;
				break;
			}
			/* Logically empty the element */
			foundElt->numChildren = 0;

			/* Release element. It should get deleted. */
			deleted = SyncHTRelease(syncHT, foundElt);
			if (!deleted)
			{
				testFailed = true;
				break;
			}
		}

//		numEntries = SyncHTNumEntries(syncHT); /* XXX This does not hold for concurrency */
//		if (numEntries != 0)
//		{
//			testFailed = true;
//			break;
//		}
	} /* for iterNo */

	unit_test_result(!testFailed);

	elog(LOG, "Running sub-test: SyncHTDestroy");
	SyncHTDestroy(syncHT);
	unit_test_result(true);

	return unit_test_summary();
}

static bool
syncrefhashtable_test_error_cases(void)
{
	unit_test_reset();

	elog(LOG, "Running test: syncrefhashtable_test_error_cases");

	elog(LOG, "Running sub-test: SyncHTCreate");
	SyncHT *syncHT = syncrefhastable_test_create();
	unit_test_result(syncHT != NULL);
	Assert(syncHT);

	elog(LOG, "Running sub-test: SyncHTInsert double remove");

	TestSyncHTElt *element;
	char *keyText = "Key one";
	bool existing;
	bool deleted;

	element = SyncHTInsert(syncHT, keyText, &existing);
	deleted = SyncHTRelease(syncHT, element);
	Assert(deleted);

	PG_TRY();
	{
		deleted = SyncHTRelease(syncHT, element);

		/* Should not get here, above line throws */
		unit_test_result(false);
	}
	PG_CATCH();
	{
		unit_test_result(true);
	}
	PG_END_TRY();

	return unit_test_summary();
}

static bool
syncrefhashtable_test_full_table(void)
{
	unit_test_reset();

	elog(LOG, "Running test: syncrefhashtable_test_error_cases");

	elog(LOG, "Running sub-test: SyncHTCreate");
	SyncHT *syncHT = syncrefhastable_test_create();
	unit_test_result(syncHT != NULL);
	Assert(syncHT);

	elog(LOG, "Running sub-test: SyncHTInsert into full hashtable");

	/* Number of elements in the array to hold test entries */
	TestSyncHTElt *elements[10 * TEST_HT_NUM_ELEMENTS + 1];
	char key[TEST_NAME_LENGTH];
	bool existing = false, deleted = false;
	bool testFailed = false;
	int i;

	/* Fill up the hashtable */
	for (i=0; i < 2 * TEST_HT_NUM_ELEMENTS; i++)
	{
		snprintf(key, TEST_NAME_LENGTH, "PID=%d key no. %d", MyProcPid, i);

		elements[i] = SyncHTInsert(syncHT, key, &existing);
		if (existing || !elements[i])
		{
			elog(LOG, "Filling up the hashtable failed after %d inserts", i + 1);
			testFailed = true;
			break;
		}
	}

	int numEntries = SyncHTNumEntries(syncHT); /* XXX This does not hold for concurrency */
	elog(LOG, "Hashtable says it hash %d entries", numEntries);

	unit_test_result(!testFailed && numEntries == TEST_HT_NUM_ELEMENTS);

	/* Insert elements in full hashtable */
	snprintf(key, TEST_NAME_LENGTH, "PID=%d key no. %d", MyProcPid, TEST_HT_NUM_ELEMENTS);
	elements[TEST_HT_NUM_ELEMENTS] = SyncHTInsert(syncHT, key, &existing);

	/* Insertion should have failed */
	unit_test_result(elements[TEST_HT_NUM_ELEMENTS] == NULL && !existing);


	/* Look-up noTestEntries elements. Release them, they should get deleted */
	for (i=0; i < TEST_HT_NUM_ELEMENTS; i++)
	{
		snprintf(key, TEST_NAME_LENGTH, "PID=%d key no. %d", MyProcPid, i);
		TestSyncHTElt *foundElt = (TestSyncHTElt *) SyncHTLookup(syncHT, key);
		if (!foundElt)
		{
			testFailed = true;
			break;
		}

		/* Release element. It should get deleted. */
		deleted = SyncHTRelease(syncHT, foundElt);
		if (!deleted)
		{
			testFailed = true;
			break;
		}
	}

	unit_test_result(!testFailed);

	return unit_test_summary();
}

/*
 * Unit test for bfz reopen operation
 */
static bool
bfz_test_reopen(void)
{
	unit_test_reset();

	elog(LOG, "Running test: bfz_test_reopen");

	StringInfo filename = makeStringInfo();

	appendStringInfo(filename,
						"%s/%s",
						PG_TEMP_FILES_DIR,
						"Test_bfz.dat");

	StringInfo text = makeStringInfo();
	appendStringInfo(text,"Small amount of data to test.");

	elog(LOG, "Running sub-test: Creating file %s", filename->data);

	/* Write data to file */
	bfz_t * fileWrite = bfz_create(filename->data, false, TRUE);
	fileWrite->del_on_close=false;
	bfz_append(fileWrite, text->data, text->len);
	/* Flush data */
	bfz_append_end(fileWrite);

	/* Read data back from file */
	char result[TEST_NAME_LENGTH];
	int result_size = 0;

	elog(LOG, "Running sub-test: Reading file %s", filename->data);
	bfz_t * fileRead = bfz_open(filename->data, true, TRUE);
	/* Seek 0 */
	bfz_scan_begin(fileRead);
	result_size = bfz_scan_next(fileRead,result,text->len);

	unit_test_result(	(result_size == text->len) &&
						(strncmp(text->data, result, text->len) == 0));

	pfree(filename->data);
	pfree(text->data);
	pfree(filename);
	pfree(text);

	return unit_test_summary();
}

/*
 * Creates a StringInfo object holding n_chars characters.
 */
static StringInfo
create_text_stringinfo(int64 n_chars)
{
	StringInfo strInfo = makeStringInfo();
	char *text = "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789";

	int64 to_write = n_chars;
	while (to_write >= (int64) strlen(text))
	{
		appendStringInfo(strInfo, "%s", text);
		to_write -= (int64) strlen(text);
	}
	Assert(to_write >= 0 && to_write < (int64) strlen(text));

	/* Pad end */
	while (to_write > 0)
	{
		appendStringInfoChar(strInfo, 'P');
		to_write--;
	}

	return strInfo;
}

/*
 * Removes a physical file. Updates the WorkfileDiskspace accounting
 *  to reflect the deletion if requested.
 * Used as part of the unit test cleanup to remove artifacts.
 *
 * path includes the pgsql_tmp path but not the filespace.
 *
 * Returns true for successful deletion, false otherwise.
 */
static bool
remove_tmp_file(char *path, int64 size, bool update_diskspace)
{
	Assert(NULL != path);

	/* Create path to file by adding crt temp path */
	StringInfo full_filepath = makeStringInfo();
	appendStringInfo(full_filepath, "%s/%s",
			getCurrentTempFilePath, path);

	/* Remove file from disk */
	int res = unlink(full_filepath->data);

	if (update_diskspace)
	{
		/* Reverting the used diskspace to reflect the deletion */
		WorkfileDiskspace_Commit(0, size, true /* update_query_space */ );
	}

	pfree(full_filepath->data);
	pfree(full_filepath);

	return (res == 0);
}

/*
 * Unit tests for new ExecWorkfile and WorkfileSegspace functionality
 *  with underlying Buffile files
 *
 * This test is only run when the per-segment limit GUC is non-zero.
 * If GUC is 0, then we don't keep track of the per-segment used size.
 *
 */
static bool
execworkfile_buffile_test(void)
{
	int64 result = 0;
	bool success = false;
	int64 expected_size = 0;
	int64 final_size = 0;
	int64 current_size = 0;
	int64 initial_diskspace = WorkfileSegspace_GetSize();

	unit_test_reset();
	elog(LOG, "Running test: execworkfile_buffile_test");
	if (0 == gp_workfile_limit_per_segment)
	{
		elog(LOG, "Skipping test because the gp_workfile_limit_per_segment is 0");
		unit_test_result(true);
		return unit_test_summary();
	}

	/* Create file name */
	char *file_name = "test_execworkfile_buffile.dat";
	StringInfo test_filepath = makeStringInfo();
	appendStringInfo(test_filepath,
					"%s/%s",
					PG_TEMP_FILES_DIR,
					file_name);

	if (test_filepath->len > MAXPGPATH)
	{
		ereport(ERROR, (errmsg("cannot generate path: %s/%s",
					PG_TEMP_FILES_DIR,
					file_name)));
	}

	elog(LOG, "Running sub-test: Creating EWF/Buffile");

	ExecWorkFile *ewf = ExecWorkFile_Create(test_filepath->data,
			BUFFILE,
			false, /* delOnClose */
			0 /* compressionType */);

	unit_test_result(ewf != NULL);

	int nchars = 100000;
	StringInfo text = create_text_stringinfo(nchars);

	elog(LOG, "Running sub-test: Writing small amount data to EWF/Buffile and checking size");

	success = ExecWorkFile_Write(ewf, text->data, 20);
	expected_size += 20;

	unit_test_result(success && expected_size == WorkfileSegspace_GetSize() - initial_diskspace);


	elog(LOG, "Running sub-test: Writing larger amount data (%d bytes) to EWF/Buffile and checking size", nchars);
	success = ExecWorkFile_Write(ewf, text->data, nchars);
	expected_size += nchars;

	unit_test_result(success && expected_size == WorkfileSegspace_GetSize() - initial_diskspace);

	elog(LOG, "Running sub-test: Writing to the middle of a EWF/Buffile and checking size");
	result = ExecWorkFile_Seek(ewf, ExecWorkFile_GetSize(ewf) / 2, SEEK_SET);
	Assert(result == 0);
	/* This write should not add to the size */
	success = ExecWorkFile_Write(ewf, text->data, ExecWorkFile_GetSize(ewf) / 10);

	unit_test_result(success && expected_size == WorkfileSegspace_GetSize() - initial_diskspace);

	elog(LOG, "Running sub-test: Seeking past end and writing data to EWF/Buffile and checking size");
	int past_end_offset = 100;
	int past_end_write = 200;
	result = ExecWorkFile_Seek(ewf, ExecWorkFile_GetSize(ewf) + past_end_offset, SEEK_SET);
	Assert(result == 0);
	success = ExecWorkFile_Write(ewf, text->data, past_end_write);
	expected_size += past_end_offset + past_end_write;

	unit_test_result(success && expected_size == WorkfileSegspace_GetSize() - initial_diskspace);

	elog(LOG, "Running sub-test: Closing EWF/Buffile");
	final_size = ExecWorkFile_Close(ewf);
	unit_test_result(final_size == expected_size);

	elog(LOG, "Running sub-test: Opening existing EWF/Buffile and checking size");

	ewf = ExecWorkFile_Open(test_filepath->data,
				BUFFILE,
				false, /* delOnClose */
				0 /* compressionType */);

	current_size = ExecWorkFile_GetSize(ewf);
	unit_test_result(current_size == final_size);

	elog(LOG, "Running sub-test: Reading from reopened EWF/Buffile file");

	int buf_size = 100;
	char *buf = (char *) palloc(buf_size);

	result = ExecWorkFile_Read(ewf, buf, buf_size);
	unit_test_result(result == buf_size);
	pfree(buf);

	elog(LOG, "Running sub-test: Closing EWF/Buffile");
	final_size = ExecWorkFile_Close(ewf);

	unit_test_result(final_size == current_size);

	elog(LOG, "Running sub-test: Removing physical file from disk");
	success = remove_tmp_file(test_filepath->data, final_size, true /* update_diskspace */);

	unit_test_result(success);

	pfree(test_filepath->data);
	pfree(test_filepath);
	pfree(text->data);
	pfree(text);

	return unit_test_summary();
}

/*
 * Unit tests for new ExecWorkfile and WorkfileSegspace functionality
 *  with underlying compressed bfz files
 *
 * This test is only run when the per-segment limit GUC is non-zero.
 * If GUC is 0, then we don't keep track of the per-segment used size.
 *
 */
static bool
execworkfile_bfz_zlib_test(void)
{
	bool success = false;
	int64 result = 0;
	int64 expected_size = 0;
	int64 current_size = 0;
	int64 final_size = 0;
	int64 initial_diskspace = WorkfileSegspace_GetSize();

	unit_test_reset();
	elog(LOG, "Running test: execworkfile_bfz_zlib_test");

	if (0 == gp_workfile_limit_per_segment)
	{
		elog(LOG, "Skipping test because the gp_workfile_limit_per_segment is 0");
		unit_test_result(true);
		return unit_test_summary();
	}

	/* Create file name */
	char *file_name = "test_execworkfile_bfz_zlib.dat";
	StringInfo test_filepath = makeStringInfo();
	appendStringInfo(test_filepath,
					"%s/%s",
					PG_TEMP_FILES_DIR,
					file_name);

	if (test_filepath->len > MAXPGPATH)
	{
		ereport(ERROR, (errmsg("cannot generate path: %s/%s",
					PG_TEMP_FILES_DIR,
					file_name)));
	}

	elog(LOG, "Running sub-test: Creating EWF/Buffile");

	ExecWorkFile *ewf = ExecWorkFile_Create(test_filepath->data,
			BFZ,
			false, /* delOnClose */
			1 /* compressionType */);

	unit_test_result(ewf != NULL);

	int nchars = 100000;
	StringInfo text = create_text_stringinfo(nchars);

	elog(LOG, "Running sub-test: Writing small amount data to EWF/BFZ and checking size");

	success = ExecWorkFile_Write(ewf, text->data, 20);
	expected_size += 20;

	unit_test_result(success && expected_size == WorkfileSegspace_GetSize() - initial_diskspace);


	elog(LOG, "Running sub-test: Writing larger amount data (%d bytes) to EWF/BFZ and checking size", nchars);
	success = ExecWorkFile_Write(ewf, text->data, nchars);
	expected_size += nchars;

	unit_test_result(success && expected_size == WorkfileSegspace_GetSize() - initial_diskspace);

	elog(LOG, "Running sub-test: Suspending EWF/BFZ and checking for size");
	final_size = ExecWorkFile_Suspend(ewf);
	unit_test_result(final_size < expected_size);

	elog(LOG, "Running sub-test: Restarting EWF/BFZ");
	ExecWorkFile_Restart(ewf);
	unit_test_result(true);

	elog(LOG, "Running sub-test: Closing EWF/BFZ");
	final_size = ExecWorkFile_Close(ewf);

	unit_test_result(final_size < expected_size);

	elog(LOG, "Running sub-test: Opening existing EWF/BFZ and checking size");

	ewf = ExecWorkFile_Open(test_filepath->data,
			BFZ,
			false, /* delOnClose */
			1 /* compressionType */);
	current_size = ExecWorkFile_GetSize(ewf);
	unit_test_result(current_size == final_size);

	elog(LOG, "Running sub-test: Reading from reopened EWF/BFZ file");

	int buf_size = 100;
	char *buf = (char *) palloc(buf_size);

	result = ExecWorkFile_Read(ewf, buf, buf_size);
	unit_test_result(result == buf_size);
	pfree(buf);

	elog(LOG, "Running sub-test: Closing EWF/BFZ");
	final_size = ExecWorkFile_Close(ewf);

	unit_test_result(final_size == current_size);

	elog(LOG, "Running sub-test: Removing physical file from disk");
	success = remove_tmp_file(test_filepath->data, final_size, true /* update_diskspace */);

	unit_test_result(success);

	pfree(test_filepath->data);
	pfree(test_filepath);
	pfree(text->data);
	pfree(text);

	return unit_test_summary();

}

/*
 * Unit tests for new ExecWorkfile and WorkfileSegspace functionality
 *  with underlying uncompressed bfz files
 *
 * This test is only run when the per-segment limit GUC is non-zero.
 * If GUC is 0, then we don't keep track of the per-segment used size.
 *
 */
static bool
execworkfile_bfz_uncompressed_test(void)
{
	bool success = false;
	int64 result = 0;
	int64 expected_size = 0;
	int64 current_size = 0;
	int64 final_size = 0;
	int64 initial_diskspace = WorkfileSegspace_GetSize();

	unit_test_reset();
	elog(LOG, "Running test: execworkfile_bfz_uncompressed_test");

	if (0 == gp_workfile_limit_per_segment)
	{
		elog(LOG, "Skipping test because the gp_workfile_limit_per_segment is 0");
		unit_test_result(true);
		return unit_test_summary();
	}

	/* Create file name */
	char *file_name = "test_execworkfile_bfz_uncomp.dat";
	StringInfo test_filepath = makeStringInfo();
	appendStringInfo(test_filepath,
					"%s/%s",
					PG_TEMP_FILES_DIR,
					file_name);

	if (test_filepath->len > MAXPGPATH)
	{
		ereport(ERROR, (errmsg("cannot generate path: %s/%s",
					PG_TEMP_FILES_DIR,
					file_name)));
	}

	elog(LOG, "Running sub-test: Creating EWF/Buffile");

	ExecWorkFile *ewf = ExecWorkFile_Create(test_filepath->data,
			BFZ,
			false, /* delOnClose */
			0 /* compressionType */);

	unit_test_result(ewf != NULL);

	int nchars = 100000;
	StringInfo text = create_text_stringinfo(nchars);

	elog(LOG, "Running sub-test: Writing small amount data to EWF/BFZ and checking size");

	success = ExecWorkFile_Write(ewf, text->data, 20);
	expected_size += 20;

	unit_test_result(success && expected_size == WorkfileSegspace_GetSize() - initial_diskspace);


	elog(LOG, "Running sub-test: Writing larger amount data (%d bytes) to EWF/BFZ and checking size", nchars);
	success = ExecWorkFile_Write(ewf, text->data, nchars);
	expected_size += nchars;

	unit_test_result(success && expected_size == WorkfileSegspace_GetSize() - initial_diskspace);

	elog(LOG, "Running sub-test: Suspending EWF/BFZ and checking for size");
	final_size = ExecWorkFile_Suspend(ewf);
	unit_test_result(final_size >= expected_size);

	elog(LOG, "Running sub-test: Restarting EWF/BFZ");
	ExecWorkFile_Restart(ewf);
	unit_test_result(true);

	elog(LOG, "Running sub-test: Closing EWF/BFZ");
	final_size = ExecWorkFile_Close(ewf);

	/*
	 * For uncompressed files, final file may contain checksums, which makes it
	 * larger than expected
	 */
	unit_test_result(final_size >= expected_size);

	elog(LOG, "Running sub-test: Opening existing EWF/BFZ and checking size");

	ewf = ExecWorkFile_Open(test_filepath->data,
			BFZ,
			false, /* delOnClose */
			0 /* compressionType */);
	current_size = ExecWorkFile_GetSize(ewf);
	unit_test_result(current_size == final_size);

	elog(LOG, "Running sub-test: Reading from reopened EWF/BFZ file");

	int buf_size = 100;
	char *buf = (char *) palloc(buf_size);

	result = ExecWorkFile_Read(ewf, buf, buf_size);
	unit_test_result(result == buf_size);
	pfree(buf);

	elog(LOG, "Running sub-test: Closing EWF/BFZ");
	final_size = ExecWorkFile_Close(ewf);

	unit_test_result(final_size == current_size);

	elog(LOG, "Running sub-test: Removing physical file from disk");
	success = remove_tmp_file(test_filepath->data, final_size, true /* update_diskspace */);

	unit_test_result(success);

	pfree(test_filepath->data);
	pfree(test_filepath);
	pfree(text->data);
	pfree(text);

	return unit_test_summary();

}

/*
 * Unit test for testing the fd.c FileDiskSize and other new capabilities
 */
static bool
fd_tests(void)
{
	unit_test_reset();
	elog(LOG, "Running test: fd_tests");

	elog(LOG, "Running sub-test: Creating fd file");

	/* Create file name */
	char *file_name = "test_fd.dat";
	StringInfo test_filepath = makeStringInfo();
	appendStringInfo(test_filepath,
					"%s/%s",
					PG_TEMP_FILES_DIR,
					file_name);

	if (test_filepath->len > MAXPGPATH)
	{
		ereport(ERROR, (errmsg("cannot generate path: %s/%s",
					PG_TEMP_FILES_DIR,
					file_name)));
	}

	File testFd = OpenNamedFile(test_filepath->data,
			true /* create */,
			false /* delOnClose */,
			true /* closeAtEOXact */);

	unit_test_result(testFd > 0);

	elog(LOG, "Running sub-test: Reading size of open empty file");
	int64 fd_size = FileDiskSize(testFd);
	unit_test_result(fd_size == 0L);

	elog(LOG, "Running sub-test: Closing file");
	FileClose(testFd);
	unit_test_result(true);

	elog(LOG, "Running sub-test: Opening existing empty file and reading size");
	testFd = OpenNamedFile(test_filepath->data,
			false /* create */,
			false /* delOnClose */,
			true /* closeAtEOXact */);

	fd_size = FileDiskSize(testFd);
	unit_test_result(fd_size == 0L);

	elog(LOG, "Running sub-test: Writing to existing open file, sync and read size");
	int nchars = 10000;
	StringInfo text = create_text_stringinfo(nchars);
	int len_to_write = 5000;
	Assert(len_to_write <= text->len);

	FileWrite(testFd, text->data, len_to_write);
	FileSync(testFd);

	fd_size = FileDiskSize(testFd);
	unit_test_result(fd_size == len_to_write);

	elog(LOG, "Running sub-test: Closing file");
	FileClose(testFd);
	unit_test_result(true);

	pfree(text->data);
	pfree(text);

	return unit_test_summary();
}

/*
 * Unit tests for the buffile size functionality
 */
static bool
buffile_size_test(void)
{
	unit_test_reset();
	elog(LOG, "Running test: buffile_size_test");

	elog(LOG, "Running sub-test: Creating buffile");

	/* Create file name */
	char *file_name = "test_buffile.dat";
	StringInfo test_filepath = makeStringInfo();
	appendStringInfo(test_filepath,
					"%s/%s",
					PG_TEMP_FILES_DIR,
					file_name);

	if (test_filepath->len > MAXPGPATH)
	{
		ereport(ERROR, (errmsg("cannot generate path: %s/%s",
					PG_TEMP_FILES_DIR,
					file_name)));
	}

	BufFile *testBf = BufFileCreateFile(test_filepath->data,
			false /* delOnClose */, false /* interXact */);

	unit_test_result(NULL != testBf);

	elog(LOG, "Running sub-test: Size of newly created buffile");
	int64 test_size = BufFileGetSize(testBf);
	unit_test_result(test_size == 0);

	elog(LOG, "Running sub-test: Writing to new buffile and reading size < bufsize");
	int nchars = 10000;
	int expected_size = nchars;
	StringInfo text = create_text_stringinfo(nchars);
	BufFileWrite(testBf, text->data, nchars);
	pfree(text->data);
	pfree(text);
	test_size = BufFileGetSize(testBf);

	unit_test_result(test_size == expected_size);

	elog(LOG, "Running sub-test: Writing to new buffile and reading size > bufsize");
	nchars = 1000000;
	expected_size += nchars;
	text = create_text_stringinfo(nchars);
	BufFileWrite(testBf, text->data, nchars);
	test_size = BufFileGetSize(testBf);

	unit_test_result(test_size == expected_size);

	elog(LOG, "Running sub-test: seeking back and writing then testing size");
	BufFileSeek(testBf, 0 /* fileno */, expected_size/2, SEEK_SET);
	/* This write should not add to the size */
	BufFileWrite(testBf, text->data, expected_size / 10);
	test_size = BufFileGetSize(testBf);

	unit_test_result(test_size == expected_size);

	elog(LOG, "Running sub-test: Closing buffile");
	BufFileClose(testBf);
	unit_test_result(true);

	elog(LOG, "Running sub-test: Opening existing and testing size");
	testBf = BufFileOpenFile(test_filepath->data,
			false /* create */,
			false /*delOnClose */,
			false /*interXact */
			);
	test_size = BufFileGetSize(testBf);

	unit_test_result(test_size == expected_size);

	elog(LOG, "Running sub-test: Seek past end, appending and testing size");
	int past_end_offset = 100;
	int past_end_write = 200;
	BufFileSeek(testBf, 0 /* fileno */, expected_size + past_end_offset, SEEK_SET);
	BufFileWrite(testBf, text->data, past_end_write);
	expected_size += past_end_offset + past_end_write;
	test_size = BufFileGetSize(testBf);

	unit_test_result(test_size == expected_size);

	elog(LOG, "Running sub-test: Closing buffile");
	BufFileClose(testBf);
	unit_test_result(true);


	pfree(text->data);
	pfree(text);

	return unit_test_summary();

}

/*
 * Unit test for the atomic functions
 *
 * These are functional tests, they only test for correctness with no concurrency
 *
 */
static bool
atomic_test(void)
{
	unit_test_reset();
	elog(LOG, "Running test: atomic_test");

	{
		elog(LOG, "Running sub-test: pg_atomic_compare_exchange_u64");
		uint64 dest = 5;
		uint64 old = 5;
		uint64 new = 6;

		elog(LOG, "Before: dest=%d, old=%d, new=%d", (uint32) dest, (uint32) old, (uint32) new);
		int32 result = pg_atomic_compare_exchange_u64((pg_atomic_uint64 *)&dest, &old, new);
		elog(LOG, "After: dest=%d, old=%d, new=%d, result=%d", (uint32) dest, (uint32) old, (uint32) new, (uint32) result);
		unit_test_result(dest == new);
	}

	{
		elog(LOG, "Running sub-test: pg_atomic_add_fetch_u64 small addition");

		int64 base = 25;
		int64 inc = 3;
		int64 result = 0;
		int64 expected_result = base + inc;
		elog(DEBUG1, "Before: base=%lld, inc=%lld, result=%lld", (long long int) base, (long long int) inc, (long long int) result);
		result = pg_atomic_add_fetch_u64((pg_atomic_uint64 *)&base, inc);
		elog(DEBUG1, "After: base=%lld, inc=%lld, result=%lld", (long long int) base, (long long int) inc, (long long int) result);
		unit_test_result(result == expected_result && base == expected_result);

		elog(LOG, "Running sub-test: pg_atomic_sub_fetch_u64 small subtraction");

		inc = 4;
		result = 0;
		expected_result = base + inc;
		elog(DEBUG1, "Before: base=%lld, inc=%lld, result=%lld", (long long int) base, (long long int) inc, (long long int) result);
		result = pg_atomic_sub_fetch_u64((pg_atomic_uint64 *)&base, inc);
		elog(DEBUG1, "After: base=%lld, inc=%lld, result=%lld", (long long int) base, (long long int) inc, (long long int) result);
		unit_test_result(result == expected_result && base == expected_result);

		elog(LOG, "Running sub-test: pg_atomic_add_fetch_u64 huge addition");
		base = 37421634719307;
		inc  = 738246483234;
		result = 0;
		expected_result = base + inc;
		elog(DEBUG1, "Before: base=%lld, inc=%lld, result=%lld", (long long int) base, (long long int) inc, (long long int) result);
		result = pg_atomic_add_fetch_u64((pg_atomic_uint64 *)&base, inc);
		elog(DEBUG1, "After: base=%lld, inc=%lld, result=%lld", (long long int) base, (long long int) inc, (long long int) result);
		unit_test_result(result == expected_result && base == expected_result);


		elog(LOG, "Running sub-test: pg_atomic_sub_fetch_u64 huge subtraction");
		inc  = 32738246483234;
		result = 0;
		expected_result = base + inc;
		elog(DEBUG1, "Before: base=%lld, inc=%lld, result=%lld", (long long int) base, (long long int) inc, (long long int) result);
		result = pg_atomic_sub_fetch_u64((pg_atomic_uint64 *)&base, inc);
		elog(DEBUG1, "After: base=%lld, inc=%lld, result=%lld", (long long int) base, (long long int) inc, (long long int) result);
		unit_test_result(result == expected_result && base == expected_result);
	}


	return unit_test_summary();
}

/*
 * Unit test for BufFile support of large files (greater than 4 GB).
 *
 */
static bool
buffile_large_file_test(void)
{
	unit_test_reset();
	elog(LOG, "Running test: buffile_large_file_test");

	StringInfo filename = makeStringInfo();

	appendStringInfo(filename,
					 "%s/%s",
					 PG_TEMP_FILES_DIR,
					 "Test_large_buff.dat");

	BufFile *bfile = BufFileCreateFile(filename->data, true /* delOnClose */, true /* interXact */);

	int nchars = 100000;
	/* 4.5 GBs */
	int total_entries = 48319;
	/* Entry that requires an int64 seek */
	int test_entry = 45000;

	StringInfo test_string = create_text_stringinfo(nchars);

	elog(LOG, "Running sub-test: Creating file %s", filename->data);

	for (int i = 0; i < total_entries; i++)
	{
		if (test_entry == i)
		{
			BufFileWrite(bfile, test_string->data , nchars*sizeof(char));
		}
		else
		{
			StringInfo text = create_text_stringinfo(nchars);

			BufFileWrite(bfile, text->data , nchars*sizeof(char));

			pfree(text->data);
			pfree(text);
		}
	}
	elog(LOG, "Running sub-test: Reading record %s", filename->data);

	char *buffer = palloc(nchars * sizeof(char));

	BufFileSeek(bfile, 0 /* fileno */, (int64) ((int64)test_entry * (int64) nchars), SEEK_SET);

	int nread = BufFileRead(bfile, buffer, nchars*sizeof(char));

	BufFileClose(bfile);

	/* Verify correct size of the inserted record and content */
	unit_test_result (nread == nchars &&
					 (strncmp(test_string->data, buffer, test_string->len) == 0));

	pfree(filename->data);
	pfree(test_string->data);
	pfree(filename);
	pfree(test_string);

	return unit_test_summary();

}

/*
 * Unit test for logical tape's support for large spill files.
 */
static bool
logicaltape_test(void)
{
	unit_test_reset();
	elog(LOG, "Running test: logicaltape_test");

	int max_tapes = 10;
	int nchars = 100000;
	/* 4.5 GBs */
	int max_entries = 48319;

	/* Target record values */
	int test_tape = 5;
	int test_entry = 45000;
	LogicalTapePos entryPos;

	LogicalTapeSet *tape_set = LogicalTapeSetCreate(max_tapes, true /*delOnclose */);

	LogicalTape *work_tape = NULL;

	StringInfo test_string = create_text_stringinfo(nchars);

	elog(LOG, "Running sub-test: Creating LogicalTape");

	/* Fill LogicalTapeSet */
	for (int i = 0; i < max_tapes; i++)
	{
		work_tape = LogicalTapeSetGetTape(tape_set, i);

		/* Create large SpillFile for LogicalTape */
		if (test_tape == i)
		{
			elog(LOG, "Running sub-test: Fill LogicalTape");
			for (int j = 0; j < max_entries; j++)
			{
				if ( j == test_entry)
				{
					/* Keep record position of target record in LogicalTape */
					LogicalTapeUnfrozenTell(tape_set, work_tape, &entryPos);

					LogicalTapeWrite(tape_set, work_tape, test_string->data, (size_t)test_string->len);
				}
				else
				{
					/* Add additional records */
					StringInfo text = create_text_stringinfo(nchars);

					LogicalTapeWrite(tape_set, work_tape, text->data, (size_t)text->len);

					pfree(text->data);
					pfree(text);
				}
			}
		}
		else
		{
			/* Add additional records */
			StringInfo text = create_text_stringinfo(nchars);

			LogicalTapeWrite(tape_set, work_tape, text->data, (size_t)text->len);

			pfree(text->data);
			pfree(text);
		}

	}

	/* Set target LogicalTape */
	work_tape = LogicalTapeSetGetTape(tape_set, test_tape);
	char *buffer = palloc(nchars * sizeof(char));

	elog(LOG, "Running sub-test: Freeze LogicalTape");
	LogicalTapeFreeze(tape_set, work_tape);

	elog(LOG, "Running sub-test: Seek in LogicalTape");
	LogicalTapeSeek(tape_set, work_tape, &entryPos);

	elog(LOG, "Running sub-test: Reading from LogicalTape");
	LogicalTapeRead(tape_set, work_tape, buffer, (size_t)(nchars*sizeof(char)));

	LogicalTapeSetClose(tape_set, NULL /* work_set */);

	unit_test_result (strncmp(test_string->data, buffer, test_string->len) == 0);

	return unit_test_summary();
}

/*
 * Unit test for testing large fd file.
 * This unit test verifies that the file's size on disk is as expected.
 */
static bool
fd_large_file_test(void)
{
	unit_test_reset();
	elog(LOG, "Running test: fd_large_file_test");

	elog(LOG, "Running sub-test: Creating fd file");

	/* Create file name */
	char *file_name = "test_large_fd.dat";
	StringInfo test_filepath = makeStringInfo();
	appendStringInfo(test_filepath,
					"%s/%s",
					PG_TEMP_FILES_DIR,
					file_name);

	if (test_filepath->len > MAXPGPATH)
	{
		ereport(ERROR, (errmsg("cannot generate path: %s/%s",
					PG_TEMP_FILES_DIR,
					file_name)));
	}

	File testFd = OpenNamedFile(test_filepath->data,
			true /* create */,
			true /* delOnClose */,
			true /* closeAtEOXact */);

	unit_test_result(testFd > 0);

	elog(LOG, "Running sub-test: Writing to existing open file, sync and read size");
	int nchars = 100000;
	/* 4.5 GBs */
	int total_entries = 48319;

	StringInfo text = create_text_stringinfo(nchars);

	for (int i = 0; i < total_entries; i++)
	{
		FileWrite(testFd, text->data, strlen(text->data));
		FileSync(testFd);
	}

	pfree(text->data);
	pfree(text);

	int64 fd_size = FileDiskSize(testFd);
	unit_test_result(fd_size == (int64) nchars * sizeof(char) * (int64) total_entries);

	elog(LOG, "Running sub-test: Closing file");
	FileClose(testFd);
	unit_test_result(true);

	pfree(test_filepath->data);
	pfree(test_filepath);

	return unit_test_summary();
}

/*
 * Unit test for writing a one MB execworkfile.
 *
 */
static bool
execworkfile_create_one_MB_file(void)
{
	unit_test_reset();
	elog(LOG, "Running test: execworkfile_one_MB_file_test");

	StringInfo filename = makeStringInfo();

	appendStringInfo(filename,
					 "%s/%s",
					 PG_TEMP_FILES_DIR,
					 "Test_buffile_one_MB_file_test.dat");

	ExecWorkFile *ewf = ExecWorkFile_Create(filename->data,
				BUFFILE,
				true, /* delOnClose */
				0 /* compressionType */);

	/* Number of characters in a MB */
	int nchars = (int)((1<<20)/sizeof(char));

	elog(LOG, "Running sub-test: Creating file %s", filename->data);

	StringInfo text = create_text_stringinfo(nchars);

	ExecWorkFile_Write(ewf, text->data, nchars*sizeof(char));

	pfree(text->data);
	pfree(text);

	elog(LOG, "Running sub-test: Closing file %s", filename->data);

	int64 final_size = workfile_mgr_close_file(NULL /* work_set */, ewf);

	/* Verify correct size of the created file */
	unit_test_result (final_size == (int64)nchars*sizeof(char) );

	pfree(filename->data);
	pfree(filename);

	return unit_test_summary();

}

/*
 * Unit test for tracking per-query disk space
 */
static bool
workfile_queryspace(void)
{

	int64 current_size = 0;
	bool success = false;

	unit_test_reset();
	elog(LOG, "Running test: workfile_queryspace");
	if (0 == gp_workfile_limit_per_query)
	{
		elog(LOG, "Skipping test because the gp_workfile_limit_per_query is 0");
		unit_test_result(true);
		return unit_test_summary();
	}

	elog(LOG, "Running sub-test: Reading initial workfile_queryspace");

	current_size = WorkfileQueryspace_GetSize(gp_session_id, gp_command_count);

	unit_test_result(current_size == 0);

	elog(LOG, "Running sub-test: Reading bogus workfile_queryspace");

	current_size = WorkfileQueryspace_GetSize(gp_session_id, gp_command_count+1);

	unit_test_result(current_size == -1);

	elog(LOG, "Running sub-test: Reserving workfile queryspace");

	int64 bytes_reserved = 12345;
	success = WorkfileQueryspace_Reserve(bytes_reserved);
	current_size = WorkfileQueryspace_GetSize(gp_session_id, gp_command_count);

	unit_test_result(success && current_size == bytes_reserved);

	elog(LOG, "Running sub-test: Committing workfile queryspace");
	int64 bytes_committed = 1234;
	WorkfileQueryspace_Commit(bytes_committed, bytes_reserved);
	current_size = WorkfileQueryspace_GetSize(gp_session_id, gp_command_count);

	unit_test_result(current_size == bytes_committed);


	return unit_test_summary();
}

/*
 * Unit test that inserts many entries in the workfile mgr shared cache
 */
static bool
workfile_fill_sharedcache(void)
{
	bool success = true;

	unit_test_reset();
	elog(LOG, "Running test: workfile_fill_sharedcache");

	int n_entries = gp_workfile_max_entries + 1;

	elog(LOG, "Running sub-test: Creating %d empty workfile sets", n_entries);

	int crt_entry = 0;
	for (crt_entry = 0; crt_entry < n_entries; crt_entry++)
	{
		workfile_set *work_set = workfile_mgr_create_set(BUFFILE,
				false /* can_be_reused */, NULL /* PlanState */);
		if (NULL == work_set)
		{
			success = false;
			break;
		}
		if (crt_entry >= gp_workfile_max_entries - 2)
		{
			/* Pause between adding extra ones so we can test from other sessions */
			elog(LOG, "Added %d entries out of %d, pausing for 30 seconds before proceeding", crt_entry + 1, n_entries);
			sleep(30);
		}

	}

	unit_test_result(success);

	return unit_test_summary();

}

/*
 * Unit test that creates very many workfiles, and then cleans them up
 */
static bool
workfile_create_and_set_cleanup(void)
{
	bool success = true;

	unit_test_reset();
	elog(LOG, "Running test: workfile_create_and_set_cleanup");

	elog(LOG, "Running sub-test: Create Workset");

	workfile_set *work_set = workfile_mgr_create_set(BUFFILE,
			false /* can_be_reused */, NULL /* PlanState */);

	unit_test_result(NULL != work_set);

	elog(LOG, "Running sub-test: Create %d workfiles", TEST_MAX_NUM_WORKFILES);

	ExecWorkFile **ewfiles = (ExecWorkFile **) palloc(TEST_MAX_NUM_WORKFILES * sizeof(ExecWorkFile *));

	for (int i=0; i < TEST_MAX_NUM_WORKFILES; i++)
	{
		ewfiles[i] = workfile_mgr_create_file(work_set);

		if (ewfiles[i] == NULL)
		{
			success = false;
			break;
		}

		if (i % 1000 == 999)
		{
			elog(LOG, "Created %d workfiles so far", i);
		}
	}
	unit_test_result(success);

	elog(LOG, "Running sub-test: Closing Workset");
	workfile_mgr_close_set(work_set);

	unit_test_result(true);

	return unit_test_summary();
}


/*
 * Unit test that creates very many workfiles, and then closes them one by one
 */
static bool
workfile_create_and_individual_cleanup(void)
{
	bool success = true;

	unit_test_reset();
	elog(LOG, "Running test: workfile_create_and_individual_cleanup");

	elog(LOG, "Running sub-test: Create Workset");

	workfile_set *work_set = workfile_mgr_create_set(BUFFILE,
			false /* can_be_reused */, NULL /* PlanState */);

	unit_test_result(NULL != work_set);

	elog(LOG, "Running sub-test: Create %d workfiles", TEST_MAX_NUM_WORKFILES);

	ExecWorkFile **ewfiles = (ExecWorkFile **) palloc(TEST_MAX_NUM_WORKFILES * sizeof(ExecWorkFile *));

	for (int i=0; i < TEST_MAX_NUM_WORKFILES; i++)
	{
		ewfiles[i] = workfile_mgr_create_file(work_set);

		if (ewfiles[i] == NULL)
		{
			success = false;
			break;
		}

		if (i % 1000 == 999)
		{
			elog(LOG, "Created %d workfiles so far", i);
		}
	}
	unit_test_result(success);

	elog(LOG, "Running sub-test: Closing %d workfiles", TEST_MAX_NUM_WORKFILES);

	for (int i=0; i < TEST_MAX_NUM_WORKFILES; i++)
	{
		workfile_mgr_close_file(work_set, ewfiles[i]);

		if (i % 1000 == 999)
		{
			elog(LOG, "Closed %d workfiles so far", i);
		}
	}
	unit_test_result(success);

	elog(LOG, "Running sub-test: Closing Workset");
	workfile_mgr_close_set(work_set);

	unit_test_result(true);

	return unit_test_summary();
}

static bool
unit_test_summary(void)
{
	elog(LOG, "Unit tests summary: PASSED: %d/%d, FAILED: %d/%d",
			tests_passed, tests_total,
			tests_failed, tests_total);
	return tests_failed == 0;
}

static void
unit_test_reset()
{
	tests_passed = tests_failed = tests_total = 0;
}

static void
unit_test_result(bool result)
{
	tests_total++;

	if (result)
	{
		tests_passed++;
		elog(LOG, "====== PASS ======");
	}
	else
	{
		tests_failed++;
		elog(LOG, "!!!!!! FAIL !!!!!!");
	}
}

static SyncHT *
syncrefhastable_test_create(void)
{
	SyncHTCtl syncHTCtl;
	MemSet(&syncHTCtl, 0, sizeof(SyncHTCtl));

	syncHTCtl.entrySize = sizeof(TestSyncHTElt);
	syncHTCtl.keySize = TEST_NAME_LENGTH;

	syncHTCtl.hash = string_hash;
	syncHTCtl.keyCopy = (HashCopyFunc) strlcpy;
	syncHTCtl.match = (HashCompareFunc) strncmp;

	syncHTCtl.numElements = TEST_HT_NUM_ELEMENTS;

	syncHTCtl.baseLWLockId = FirstWorkfileMgrLock;
	syncHTCtl.numPartitions = NUM_WORKFILEMGR_PARTITIONS;

	syncHTCtl.tabName = "Test SyncRef Hashtable";
	syncHTCtl.isEmptyEntry = isTestEltEmpty;
	syncHTCtl.initEntry = initTestElt;

	syncHTCtl.keyOffset = GPDB_OFFSET(TestSyncHTElt, key);
	syncHTCtl.pinCountOffset = GPDB_OFFSET(TestSyncHTElt, pinCount);

	/* Create hashtable in the top memory context so we can clean up if transaction aborts */
	MemoryContext oldcxt = MemoryContextSwitchTo(TopMemoryContext);

	SyncHT *syncHT = SyncHTCreate(&syncHTCtl);
	Assert(syncHT);

	MemoryContextSwitchTo(oldcxt);

	return syncHT;
}

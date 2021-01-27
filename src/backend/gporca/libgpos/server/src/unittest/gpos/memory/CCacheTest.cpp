//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC CORP.
//
//	@filename:
//		CCacheTest.cpp
//
//	@doc:
//		Tests for CCache
//---------------------------------------------------------------------------

#include "unittest/gpos/memory/CCacheTest.h"

#include "gpos/common/CAutoP.h"
#include "gpos/common/CRandom.h"
#include "gpos/error/CAutoTrace.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/memory/CCacheFactory.h"
#include "gpos/task/CAutoTaskProxy.h"
#include "gpos/test/CUnittest.h"


using namespace gpos;

#define GPOS_CACHE_THREADS 10
#define GPOS_CACHE_ELEMENTS 20
#define GPOS_CACHE_DUPLICATES 5
#define GPOS_CACHE_DUPLICATES_TO_DELETE 3

// static variable
static BOOL fUnique = true;

//---------------------------------------------------------------------------
//	@function:
//		CCacheTest::EresUnittest
//
//	@doc:
//		Driver of cache unittests
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCacheTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CCacheTest::EresUnittest_Basic),
		GPOS_UNITTEST_FUNC(CCacheTest::EresUnittest_Refcount),
		GPOS_UNITTEST_FUNC(CCacheTest::EresUnittest_Eviction),
		GPOS_UNITTEST_FUNC(CCacheTest::EresUnittest_Iteration),
		GPOS_UNITTEST_FUNC(CCacheTest::EresUnittest_DeepObject),
		GPOS_UNITTEST_FUNC(CCacheTest::EresUnittest_IterativeDeletion)};

	fUnique = true;
	GPOS_RESULT eres = CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));

	if (GPOS_OK == eres)
	{
		fUnique = false;
		eres = CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
	}

	return eres;
}


//---------------------------------------------------------------------------
//	@function:
//		CCacheTest::SSimpleObject::FMyEqual
//
//	@doc:
//		Key equality function
//
//---------------------------------------------------------------------------
BOOL
CCacheTest::SSimpleObject::FMyEqual(ULONG *const &pvKey,
									ULONG *const &pvKeySecond)
{
	BOOL fReturn = false;

	if (NULL == pvKey && NULL == pvKeySecond)
	{
		fReturn = true;
	}
	else if (NULL == pvKey || NULL == pvKeySecond)
	{
		fReturn = false;
	}
	else
	{
		fReturn = (*pvKey) == (*pvKeySecond);
	}

	return fReturn;
}


//---------------------------------------------------------------------------
//	@function:
//		CCacheTest::CDeepObject::UlMyHash
//
//	@doc:
//		The hash code of linked list is the summation of entries' keys
//
//---------------------------------------------------------------------------
ULONG
CCacheTest::CDeepObject::UlMyHash(CDeepObject::CDeepObjectList *const &plist)
{
	ULONG ulKey = 0;
	SDeepObjectEntry *pdoe = plist->First();
	while (pdoe != NULL)
	{
		ulKey += pdoe->m_ulKey;
		pdoe = plist->Next(pdoe);
	}

	return ulKey;
}


//---------------------------------------------------------------------------
//	@function:
//		CCacheTest::CDeepObject::FMyEqual
//
//	@doc:
//		Two objects are equal if the keys in their corresponding list
//		entries are equal
//
//---------------------------------------------------------------------------
BOOL
CCacheTest::CDeepObject::FMyEqual(
	CDeepObject::CDeepObjectList *const &plist,
	CDeepObject::CDeepObjectList *const &plistSecond)
{
	BOOL fReturn = false;
	if (NULL == plist && NULL == plistSecond)
	{
		fReturn = true;
	}
	else if (NULL == plist || NULL == plistSecond)
	{
		fReturn = false;
	}
	else
	{
		if (plist->Size() != plistSecond->Size())
		{
			fReturn = false;
		}
		else
		{
			fReturn = true;
			SDeepObjectEntry *pdoe = plist->First();
			SDeepObjectEntry *pdoeSecond = plistSecond->First();
			while (NULL != pdoe)
			{
				GPOS_ASSERT(NULL != pdoeSecond &&
							"Reached a NULL entry in the second list");

				if (pdoe->m_ulKey != pdoeSecond->m_ulKey)
				{
					fReturn = false;
					break;
				}
				pdoe = plist->Next(pdoe);
				pdoeSecond = plistSecond->Next(pdoeSecond);
			}
		}
	}

	return fReturn;
}


//---------------------------------------------------------------------------
//	@function:
//		CCacheTest::CDeepObject::AddEntry
//
//	@doc:
//		Adds a new entry to the linked list
//
//---------------------------------------------------------------------------
void
CCacheTest::CDeepObject::AddEntry(CMemoryPool *mp, ULONG ulKey, ULONG ulVal)
{
	m_list.Prepend(GPOS_NEW(mp) SDeepObjectEntry(ulKey, ulVal));
}


//---------------------------------------------------------------------------
//	@function:
//		CCacheTest::EresUnittest_Basic
//
//	@doc:
//		Basic cache test (insert, lookup, delete, lookup again)
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCacheTest::EresUnittest_Basic()
{
	CAutoP<CCache<SSimpleObject *, ULONG *> > apcache;
	apcache = CCacheFactory::CreateCache<SSimpleObject *, ULONG *>(
		fUnique, UNLIMITED_CACHE_QUOTA, SSimpleObject::UlMyHash,
		SSimpleObject::FMyEqual);

	CCache<SSimpleObject *, ULONG *> *pcache = apcache.Value();

	//insertion - scope for accessor
	{
		CSimpleObjectCacheAccessor ca(pcache);

		SSimpleObject *pso = GPOS_NEW(ca.Pmp()) SSimpleObject(1, 2);

#ifdef GPOS_DEBUG
		SSimpleObject *psoReturned =
#endif	// GPOS_DEBUG
			ca.Insert(&(pso->m_ulKey), pso);

		//release the ownership from pso, but ccacheentry still has the ownership
		pso->Release();

		GPOS_ASSERT(psoReturned == pso && "Incorrect cache entry was inserted");
		GPOS_ASSERT(1 == pcache->Size());

		// insert duplicate while not allowed
		if (pcache->AllowsDuplicateKeys())
		{
			CSimpleObjectCacheAccessor ca(pcache);
			SSimpleObject *psoDuplicate =
				GPOS_NEW(ca.Pmp()) SSimpleObject(1, 5);

#ifdef GPOS_DEBUG
			SSimpleObject *psoReturned =
#endif	// GPOS_DEBUG
				ca.Insert(&(psoDuplicate->m_ulKey), psoDuplicate);

			GPOS_ASSERT(psoReturned == pso && "Duplicate insertion must fail");
			GPOS_ASSERT(1 == pcache->Size());

			// delete original cache object
			ca.MarkForDeletion();
		}

		// lookup - scope for accessor
		{
			CSimpleObjectCacheAccessor ca(pcache);
			ULONG ulkey = 1;
			ca.Lookup(&ulkey);
			pso = ca.Val();

			if (NULL != pso)
			{
				// release object since there is no customer to release it after lookup and before CCache's cleanup
				pso->Release();
			}

			GPOS_ASSERT_IMP(!pcache->AllowsDuplicateKeys(),
							NULL != pso && 2 == pso->m_ulValue);
			GPOS_ASSERT_IMP(pcache->AllowsDuplicateKeys(), NULL == pso);
		}

		// delete - scope for accessor
		{
			CSimpleObjectCacheAccessor ca(pcache);
			ULONG ulkey = 1;
			ca.Lookup(&ulkey);
			pso = ca.Val();

			GPOS_ASSERT_IMP(!pcache->AllowsDuplicateKeys(), NULL != pso);

			if (NULL != pso)
			{
				// release object since there is no customer to release it after lookup and before CCache's cleanup
				pso->Release();

				ca.MarkForDeletion();
			}
		}

		// lookup again - scope for accessor
		{
			CSimpleObjectCacheAccessor ca(pcache);
			ULONG ulkey = 1;
			ca.Lookup(&ulkey);
			pso = ca.Val();

			GPOS_ASSERT(NULL == pso);
		}

		// at this point, we still maintain a valid cached object held by the
		// outmost accessor

		GPOS_ASSERT(NULL != psoReturned && 2 == psoReturned->m_ulValue);
	}

	GPOS_ASSERT(0 == pcache->Size());

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CCacheTest::EresUnittest_Refcount
//
//	@doc:
//		Basic Ref count test:
//			Insert Ref Count object, dec ref count of the object, and get the object from cache
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCacheTest::EresUnittest_Refcount()
{
	CAutoP<CCache<SSimpleObject *, ULONG *> > apcache;
	apcache = CCacheFactory::CreateCache<SSimpleObject *, ULONG *>(
		fUnique, UNLIMITED_CACHE_QUOTA, SSimpleObject::UlMyHash,
		SSimpleObject::FMyEqual);

	CCache<SSimpleObject *, ULONG *> *pcache = apcache.Value();
	SSimpleObject *pso = NULL;
	//Scope of the accessor when we insert
	{
		CSimpleObjectCacheAccessor ca(pcache);
		CMemoryPool *mp = ca.Pmp();

		pso = GPOS_NEW(mp) SSimpleObject(1, 2);
		GPOS_ASSERT(1 == pso->RefCount());

#ifdef GPOS_DEBUG
		SSimpleObject *psoReturned =
#endif	// GPOS_DEBUG
			ca.Insert(&(pso->m_ulKey), pso);

		// 1 by CRefCount, 2 by CCacheEntry constructor and 3 by CCache Accessor
		GPOS_ASSERT(3 == pso->RefCount() && "Expected refcount to be 3");
		GPOS_ASSERT(psoReturned == pso && "Incorrect cache entry was inserted");
	}

	GPOS_ASSERT(
		2 == pso->RefCount() &&
		"Expected refcount to be 2 because CCacheAccessor goes out of scope");

	{
		//Create new access for lookup
		CSimpleObjectCacheAccessor ca(pcache);

		GPOS_ASSERT(2 == pso->RefCount() &&
					"Expected pso and CCacheEntry to have ownership");

		//Ideally Lookup should return valid object. Until CCache evict and no one has reference to it, this object can't be deleted
		ca.Lookup(&(pso->m_ulKey));

		// 1 by CRefCount, 2 by CCacheEntry constructor, 3 by CCache Accessor, 4 by Lookup
		GPOS_ASSERT(
			4 == pso->RefCount() &&
			"Expected pso, CCacheEntry CCacheAccessor, and customer to have ownership");
		// Ideally it shouldn't delete itself because CCache is still holding this object
		pso->Release();
		GPOS_ASSERT(
			3 == pso->RefCount() &&
			"Expected CCacheEntry, CCacheAccessor and customer to have ownership");
	}
	GPOS_ASSERT(
		2 == pso->RefCount() &&
		"Expected refcount to be 1. CCacheEntry and customer have the ownership");

	// release object since there is no customer to release it after lookup and before CCache's cleanup
	pso->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CCacheTest::InsertOneElement
//
//	@doc:
//		Inserts one SSimpleObject with both key and value set to ulKey.
//		Returns the size of a single instance of SSimpleObject
//
//---------------------------------------------------------------------------
ULLONG
CCacheTest::InsertOneElement(CCache<SSimpleObject *, ULONG *> *pCache,
							 ULONG ulKey)
{
	ULLONG ulTotalAllocatedSize = 0;
	SSimpleObject *pso = NULL;
	{
		CSimpleObjectCacheAccessor ca(pCache);
		CMemoryPool *mp = ca.Pmp();
		pso = GPOS_NEW(mp) SSimpleObject(ulKey, ulKey);
		ca.Insert(&(pso->m_ulKey), pso);
		GPOS_ASSERT(
			3 == pso->RefCount() &&
			"Expected pso, cacheentry and cacheaccessor to have ownership");
		//Remove the ownership of pso. Still CCacheEntry has the ownership
		pso->Release();
		GPOS_ASSERT(2 == pso->RefCount() &&
					"Expected pso and cacheentry to have ownership");
		ulTotalAllocatedSize = mp->TotalAllocatedSize();
	}
	GPOS_ASSERT(1 == pso->RefCount() &&
				"Expected only cacheentry to have ownership");
	return ulTotalAllocatedSize;
}

//---------------------------------------------------------------------------
//	@function:
//		CCacheTest::ULFillCacheWithoutEviction
//
//	@doc:
//		Inserts as many SSimpleObjects as needed (starting with the key ulKeyStart and
//		sequentially generating the successive keys) to consume cache quota.
//		Returns the key of the last inserted element
//---------------------------------------------------------------------------
ULONG
CCacheTest::ULFillCacheWithoutEviction(CCache<SSimpleObject *, ULONG *> *pCache,
									   ULONG ulKeyStart)
{
#ifdef GPOS_DEBUG
	// initial size of the cache
	ULLONG ullInitialCacheSize = pCache->TotalAllocatedSize();
	ULLONG ullOldEvictionCounter = pCache->GetEvictionCounter();
#endif

	ULLONG ullOneElemSize = InsertOneElement(pCache, ulKeyStart);

#ifdef GPOS_DEBUG
	ULLONG ullOneElemCacheSize = pCache->TotalAllocatedSize();
	ULLONG ullNewEvictionCounter = pCache->GetEvictionCounter();
#endif

	GPOS_ASSERT((ullOneElemCacheSize > ullInitialCacheSize ||
				 ullOldEvictionCounter < ullNewEvictionCounter) &&
				"Cache size didn't change upon insertion");

	ULLONG ullCacheCapacity = pCache->GetCacheQuota() / ullOneElemSize;

	// We already have an element in the cache and the eviction happens after we violate.
	// So, we should not trigger eviction inserting cacheCapacity + 1
	for (ULONG ulElemCount = 1; ulElemCount <= ullCacheCapacity; ulElemCount++)
	{
		InsertOneElement(pCache, ulKeyStart + ulElemCount);
		GPOS_CHECK_ABORT;
	}

#ifdef GPOS_DEBUG
	ULLONG ullSizeBeforeEviction =
#endif	// GPOS_DEBUG
		pCache->TotalAllocatedSize();

	// Check the size of the cache. Nothing should be evicted if the cache was initially empty
#ifdef GPOS_DEBUG
	ULLONG ullExpectedCacheSize = (ullCacheCapacity + 1) * ullOneElemSize;
#endif	// GPOS_DEBUG

	GPOS_ASSERT_IMP(
		0 == ullInitialCacheSize,
		ullSizeBeforeEviction == ullExpectedCacheSize &&
			ullSizeBeforeEviction + ullOneElemSize > ullInitialCacheSize);

	return (ULONG)(ullCacheCapacity + ulKeyStart);
}

//---------------------------------------------------------------------------
//	@function:
//		CCacheTest::CheckGenerationSanityAfterEviction
//
//	@doc:
//		Checks if after eviction we have more entries from newer generation than the older generation
//---------------------------------------------------------------------------
void
CCacheTest::CheckGenerationSanityAfterEviction(
	CCache<SSimpleObject *, ULONG *> *pCache,
	ULLONG
#ifdef GPOS_DEBUG
		ullOneElemSize
#endif
	,
	ULONG ulOldGenBeginKey, ULONG ulOldGenEndKey, ULONG ulNewGenEndKey)
{
	ULONG uloldGenEntryCount = 0;
	ULONG ulNewGenEntryCount = 0;

	for (ULONG ulKey = ulOldGenBeginKey; ulKey <= ulNewGenEndKey; ulKey++)
	{
		CSimpleObjectCacheAccessor ca(pCache);
		ca.Lookup(&ulKey);
		SSimpleObject *pso = ca.Val();
		if (NULL != pso)
		{
			// release object since there is no customer to release it after lookup and before CCache's cleanup
			pso->Release();

			if (ulKey <= ulOldGenEndKey)
			{
				uloldGenEntryCount++;
			}
			else
			{
				ulNewGenEntryCount++;
			}
		}
	}

#ifdef GPOS_DEBUG
	ULLONG ullCacheCapacity = pCache->GetCacheQuota() / ullOneElemSize;
#endif

	// total in-cache entries must be at least as many as the minimum number of in-cache entries after an eviction
	GPOS_ASSERT(
		uloldGenEntryCount + ulNewGenEntryCount >=
		(ULONG)((double) ullCacheCapacity * (1 - pCache->GetEvictionFactor())));
	// there should be at least as many new gen entries as the old gen entries as they get to live longer
	GPOS_ASSERT(ulNewGenEntryCount >= uloldGenEntryCount);
}

//---------------------------------------------------------------------------
//	@function:
//		CCacheTest::TestEvictionForOneCacheSize
//
//	@doc:
//		Tests if cache eviction works for a single cache size
//---------------------------------------------------------------------------
void
CCacheTest::TestEvictionForOneCacheSize(ULLONG ullCacheQuota)
{
	CAutoP<CCache<SSimpleObject *, ULONG *> > apCache;
	apCache = CCacheFactory::CreateCache<SSimpleObject *, ULONG *>(
		false, /* not an unique cache */
		ullCacheQuota, SSimpleObject::UlMyHash, SSimpleObject::FMyEqual);

	CCache<SSimpleObject *, ULONG *> *pCache = apCache.Value();
	ULONG ulLastKeyFirstGen = ULFillCacheWithoutEviction(pCache, 0);

#ifdef GPOS_DEBUG
	ULLONG ullSizeBeforeEviction = pCache->TotalAllocatedSize();
#endif

	ULLONG ullOneElemSize = InsertOneElement(pCache, ulLastKeyFirstGen + 1);

#ifdef GPOS_DEBUG
	ULLONG ullPostEvictionSize = pCache->TotalAllocatedSize();
#endif

	// Make sure cache is now smaller, due to eviction
	GPOS_ASSERT(ullPostEvictionSize < ullSizeBeforeEviction);
	// Now insert another batch of elements to fill the cache
	ULONG ulLastKeySecondGen =
		ULFillCacheWithoutEviction(pCache, ulLastKeyFirstGen + 2);
	// Another batch of insert to the cache's filling should evict all the second generation keys
	ULONG ulLastKeyThirdGen =
		ULFillCacheWithoutEviction(pCache, ulLastKeySecondGen + 1);

	CSimpleObjectCacheAccessor caBeforeEviction(pCache);
	// this is now pinned as the accessor is not going out of scope; pinned entry is used later for checking non-eviction
	caBeforeEviction.Lookup(&ulLastKeyThirdGen);

	SSimpleObject *psoBeforeEviction = caBeforeEviction.Val();

	if (NULL != psoBeforeEviction)
	{
		// release object since there is no customer to release it after lookup and before CCache's cleanup
		psoBeforeEviction->Release();
	}

	// Now verify everything from the first generation insertion is evicted
	for (ULONG ulKey = 0; ulKey <= ulLastKeyFirstGen; ulKey++)
	{
		CSimpleObjectCacheAccessor ca(pCache);
		ca.Lookup(&ulKey);

		SSimpleObject *pso = ca.Val();

		if (NULL != pso)
		{
			// release object since there is no customer to release it after lookup and before CCache's cleanup
			pso->Release();
		}

		GPOS_ASSERT(NULL == pso);
	}


	// now ensure that newer gen items are outliving older gen during cache eviction
	CheckGenerationSanityAfterEviction(pCache, ullOneElemSize,
									   ulLastKeyFirstGen + 2,
									   ulLastKeySecondGen, ulLastKeyThirdGen);

	ULLONG ullNewQuota =
		static_cast<ULLONG>(static_cast<double>(ullCacheQuota) * 0.5);
	// drastically reduce the size of the cache
	pCache->SetCacheQuota(ullNewQuota);
	GPOS_ASSERT(pCache->GetCacheQuota() == ullNewQuota);
	// now ensure that newer gen items are outliving older gen during cache eviction
	CheckGenerationSanityAfterEviction(pCache, ullOneElemSize,
									   ulLastKeyFirstGen + 2,
									   ulLastKeySecondGen, ulLastKeyThirdGen);

	// now check pinning would retain the entry, no matter how many eviction is triggered

	// Another batch of insert to the cache's filling should evict all the third generation keys
	ULONG ulLastKeyFourthGen =
		ULFillCacheWithoutEviction(pCache, ulLastKeyThirdGen + 1);

	// Another batch of insert to the cache's filling should evict all the forth generation keys
	ULONG ulLastKeyFifthGen =
		ULFillCacheWithoutEviction(pCache, ulLastKeyFourthGen + 1);

	// Another batch of insert to the cache's filling should evict all the fifth generation keys
	ULFillCacheWithoutEviction(pCache, ulLastKeyFifthGen + 1);
	for (ULONG ulKey = ulLastKeySecondGen + 1; ulKey <= ulLastKeyFourthGen;
		 ulKey++)
	{
		CSimpleObjectCacheAccessor ca(pCache);
		ca.Lookup(&ulKey);

		SSimpleObject *pso = ca.Val();

		if (NULL != pso)
		{
			// release object since there is no customer to release it after lookup and before CCache's cleanup
			pso->Release();
		}

		// everything is evicted from third and fourth gen, except the pinned entry
		GPOS_ASSERT_IMP(ulKey != ulLastKeyThirdGen, NULL == pso);
		GPOS_ASSERT_IMP(ulKey == ulLastKeyThirdGen, NULL != pso);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CCacheTest::EresUnittest_Eviction
//
//	@doc:
//		Test if cache eviction works
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCacheTest::EresUnittest_Eviction()
{
	TestEvictionForOneCacheSize(10240);
	TestEvictionForOneCacheSize(20480);

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CCacheTest::EresInsertDuplicates
//
//	@doc:
//		Helper function: insert multiple elements with duplicate keys
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCacheTest::EresInsertDuplicates(CCache<SSimpleObject *, ULONG *> *pcache)
{
	ULONG ulDuplicates = 1;
	if (!pcache->AllowsDuplicateKeys())
	{
		ulDuplicates = GPOS_CACHE_DUPLICATES;
	}

	for (ULONG i = 0; i < GPOS_CACHE_ELEMENTS; i++)
	{
		for (ULONG j = 0; j < ulDuplicates; j++)
		{
			CSimpleObjectCacheAccessor ca(pcache);
			SSimpleObject *pso = GPOS_NEW(ca.Pmp()) SSimpleObject(i, j);

#ifdef GPOS_DEBUG
			SSimpleObject *psoReturned =
#endif	// GPOS_DEBUG
				ca.Insert(&(pso->m_ulKey), pso);

			GPOS_ASSERT(NULL != psoReturned);

			pso->Release();
		}
		GPOS_CHECK_ABORT;
	}

	{
		CAutoMemoryPool amp;
		CMemoryPool *mp = amp.Pmp();
		CAutoTrace at(mp);
		at.Os() << std::endl
				<< "Total memory consumption by cache: "
				<< pcache->TotalAllocatedSize() << " bytes";
		at.Os() << std::endl
				<< "Total memory consumption by memory manager: "
				<< CMemoryPoolManager::GetMemoryPoolMgr()->TotalAllocatedSize()
				<< " bytes";
	}

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CCacheTest::EresRemoveDuplicates
//
//	@doc:
//		Helper function: remove multiple elements with the same key
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCacheTest::EresRemoveDuplicates(CCache<SSimpleObject *, ULONG *> *pcache)
{
	for (ULONG i = 0; i < GPOS_CACHE_ELEMENTS; i++)
	{
		GPOS_CHECK_ABORT;

		CSimpleObjectCacheAccessor ca(pcache);
		ca.Lookup(&i);
		ULONG count = 0;
		SSimpleObject *pso = ca.Val();
		GPOS_ASSERT(NULL != pso);

		if (NULL != pso)
		{
			// release object since there is no customer to release it after lookup and before CCache's cleanup
			pso->Release();
		}

		while (NULL != pso)
		{
			GPOS_CHECK_ABORT;

			GPOS_ASSERT(pso->m_ulValue < GPOS_CACHE_DUPLICATES &&
						"Incorrect entry was found");

			if (pso->m_ulValue < GPOS_CACHE_DUPLICATES_TO_DELETE)
			{
				ca.MarkForDeletion();
				count++;
			}

			pso = ca.Next();
		}
		GPOS_ASSERT(count == GPOS_CACHE_DUPLICATES_TO_DELETE &&
					"Incorrect number of deleted entries");
	}

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CCacheTest::EresUnittest_DeepObject
//
//	@doc:
//		Cache test with deep objects
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCacheTest::EresUnittest_DeepObject()
{
	CAutoMemoryPool amp(CAutoMemoryPool::ElcNone);

	// construct a key
	CDeepObject *pdoDummy = GPOS_NEW(amp.Pmp()) CDeepObject();
	pdoDummy->AddEntry(amp.Pmp(), 1, 1);
	pdoDummy->AddEntry(amp.Pmp(), 2, 2);

	CAutoP<CCache<CDeepObject *, CDeepObject::CDeepObjectList *> > apcache;
	apcache = CCacheFactory::CreateCache<CDeepObject *,
										 CDeepObject::CDeepObjectList *>(
		fUnique, UNLIMITED_CACHE_QUOTA, &CDeepObject::UlMyHash,
		&CDeepObject::FMyEqual);

	CCache<CDeepObject *, CDeepObject::CDeepObjectList *> *pcache =
		apcache.Value();

	// insertion - scope for accessor
	{
		CDeepObjectCacheAccessor ca(pcache);
		CMemoryPool *mp = ca.Pmp();
		CDeepObject *pdo = GPOS_NEW(mp) CDeepObject();
		pdo->AddEntry(mp, 1, 1);
		pdo->AddEntry(mp, 2, 2);

#ifdef GPOS_DEBUG
		CDeepObject *pdoReturned =
#endif	// GPOS_DEBUG
			ca.Insert(pdo->Key(), pdo);
		pdo->Release();

		GPOS_ASSERT(NULL != pdoReturned &&
					"Incorrect cache entry was inserted");

		// insert duplicate while not allowed
		if (pcache->AllowsDuplicateKeys())
		{
			CDeepObjectCacheAccessor ca(pcache);
			CMemoryPool *mp = ca.Pmp();
			CDeepObject *pdoDuplicate = GPOS_NEW(mp) CDeepObject();
			pdoDuplicate->AddEntry(mp, 1, 5);
			pdoDuplicate->AddEntry(mp, 2, 5);

#ifdef GPOS_DEBUG
			CDeepObject *pdoReturned =
#endif	// GPOS_DEBUG
				ca.Insert(pdoDuplicate->Key(), pdoDuplicate);

			GPOS_ASSERT(pdoReturned == pdo && "Duplicate insertion must fail");

			// delete original cache object
			ca.MarkForDeletion();
		}


		// lookup - scope for accessor
		{
			CDeepObjectCacheAccessor ca(pcache);
			ca.Lookup(pdoDummy->Key());
			pdo = ca.Val();

			if (NULL != pdo)
			{
				// release object since there is no customer to release it after lookup and before CCache's cleanup
				pdo->Release();
			}

			GPOS_ASSERT_IMP(pcache->AllowsDuplicateKeys(), NULL == pdo);
			GPOS_ASSERT_IMP(!pcache->AllowsDuplicateKeys(), NULL != pdo);
			GPOS_ASSERT_IMP(!pcache->AllowsDuplicateKeys(),
							3 == CDeepObject::UlMyHash(pdo->Key()) &&
								"Incorrect cache entry");
		}

		// delete - scope for accessor
		{
			CDeepObjectCacheAccessor ca(pcache);
			ca.Lookup(pdoDummy->Key());
			pdo = ca.Val();

			GPOS_ASSERT_IMP(!pcache->AllowsDuplicateKeys(), NULL != pdo);

			if (NULL != pdo)
			{
				// release object since there is no customer to release it after lookup and before CCache's cleanup
				pdo->Release();

				ca.MarkForDeletion();
			}
		}

		// lookup again - scope for accessor
		{
			CDeepObjectCacheAccessor ca(pcache);
			ca.Lookup(pdoDummy->Key());
			pdo = ca.Val();
			GPOS_ASSERT(NULL == pdo);
		}

		// at this point, we still maintain a valid cached object held by the
		// outmost accessor

		GPOS_ASSERT(NULL != pdoReturned &&
					3 == CDeepObject::UlMyHash(pdoReturned->Key()));
	}

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CCacheTest::EresUnittest_Iteration
//
//	@doc:
//		Cache iteration test
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCacheTest::EresUnittest_Iteration()
{
	CAutoP<CCache<SSimpleObject *, ULONG *> > apcache;
	apcache = CCacheFactory::CreateCache<SSimpleObject *, ULONG *>(
		fUnique, UNLIMITED_CACHE_QUOTA, SSimpleObject::UlMyHash,
		SSimpleObject::FMyEqual);

	CCache<SSimpleObject *, ULONG *> *pcache = apcache.Value();

	CCacheTest::EresInsertDuplicates(pcache);

#ifdef GPOS_DEBUG
	ULONG ulDuplicates = 1;
	if (!pcache->AllowsDuplicateKeys())
	{
		ulDuplicates = GPOS_CACHE_DUPLICATES;
	}
#endif	// GPOS_DEBUG

	for (ULONG i = 0; i < GPOS_CACHE_ELEMENTS; i++)
	{
		GPOS_CHECK_ABORT;

		CSimpleObjectCacheAccessor ca(pcache);
		ca.Lookup(&i);
		ULONG count = 0;
		SSimpleObject *pso = ca.Val();
		GPOS_ASSERT(NULL != pso);

		// release object since there is no customer to release it after lookup and before CCache's cleanup
		pso->Release();

		while (NULL != pso)
		{
			GPOS_CHECK_ABORT;

			GPOS_ASSERT(ulDuplicates > pso->m_ulValue &&
						"Incorrect entry was found");

			count++;
			pso = ca.Next();
		}
		GPOS_ASSERT(count == ulDuplicates && "Incorrect number of duplicates");
	}

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CCacheTest::EresUnittest_IterativeDeletion
//
//	@doc:
//		Cache iterative deletion test
//
//---------------------------------------------------------------------------
GPOS_RESULT
CCacheTest::EresUnittest_IterativeDeletion()
{
	GPOS_ASSERT(GPOS_CACHE_DUPLICATES >= GPOS_CACHE_DUPLICATES_TO_DELETE);

	CAutoP<CCache<SSimpleObject *, ULONG *> > apcache;
	apcache = CCacheFactory::CreateCache<SSimpleObject *, ULONG *>(
		fUnique, UNLIMITED_CACHE_QUOTA, SSimpleObject::UlMyHash,
		SSimpleObject::FMyEqual);

	CCache<SSimpleObject *, ULONG *> *pcache = apcache.Value();

	CCacheTest::EresInsertDuplicates(pcache);

	if (!pcache->AllowsDuplicateKeys())
	{
		CCacheTest::EresRemoveDuplicates(pcache);
	}

#ifdef GPOS_DEBUG
	ULONG ulDuplicates = 1;
	ULONG ulDuplicatesToDelete = 0;
	if (!pcache->AllowsDuplicateKeys())
	{
		ulDuplicates = GPOS_CACHE_DUPLICATES;
		ulDuplicatesToDelete = GPOS_CACHE_DUPLICATES_TO_DELETE;
	}

	ULONG ulRemaining = ulDuplicates - ulDuplicatesToDelete;
#endif	// GPOS_DEBUG

	// count remaining duplicate entries
	for (ULONG i = 0; i < GPOS_CACHE_ELEMENTS; i++)
	{
		GPOS_CHECK_ABORT;

		CSimpleObjectCacheAccessor ca(pcache);
		ca.Lookup(&i);
		ULONG count = 0;
		SSimpleObject *pso = ca.Val();
		GPOS_ASSERT_IMP(0 < ulRemaining, NULL != pso);

		if (NULL != pso)
		{
			// release object since there is no customer to release it after lookup and before CCache's cleanup
			pso->Release();
		}

		while (NULL != pso)
		{
			GPOS_CHECK_ABORT;

			GPOS_ASSERT(pso->m_ulValue >= ulDuplicatesToDelete &&
						"Incorrect entry value was found");
			count++;
			pso = ca.Next();
		}

		GPOS_ASSERT(count == ulRemaining &&
					"Incorrect number of remaining duplicates");
	}

	return GPOS_OK;
}

// EOF

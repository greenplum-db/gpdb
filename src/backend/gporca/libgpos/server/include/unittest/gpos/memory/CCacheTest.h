//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC CORP.
//
//	@filename:
//		CCacheTest.h
//
//	@doc:
//		Test for CCache
//---------------------------------------------------------------------------
#ifndef GPOS_CCACHETEST_H_
#define GPOS_CCACHETEST_H_

#include "gpos/common/CList.h"
#include "gpos/memory/CCache.h"
#include "gpos/memory/CCacheAccessor.h"


namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CCacheTest
//
//	@doc:
//		Static unit tests
//
//---------------------------------------------------------------------------
class CCacheTest
{
private:
	// A simple object (no deep structures)
	struct SSimpleObject : public CRefCount
	{
		ULONG m_ulKey;

		ULONG m_ulValue;

		SSimpleObject(ULONG ulKey, ULONG ulVal)
			: m_ulKey(ulKey), m_ulValue(ulVal)
		{
		}

		static ULONG
		UlMyHash(ULONG *const &pvKey)
		{
			return *pvKey;
		}

		//key equality function
		static BOOL FMyEqual(ULONG *const &pvKey, ULONG *const &pvKeySecond);

		// equality for object-based comparison
		BOOL
		operator==(const SSimpleObject &obj) const
		{
			return obj.m_ulKey == m_ulKey;
		}
	};	// struct SSimpleObject

	// helper functions

	// insert elements with duplicate keys
	static GPOS_RESULT EresInsertDuplicates(CCache<SSimpleObject *, ULONG *> *);

	// remove
	static GPOS_RESULT EresRemoveDuplicates(CCache<SSimpleObject *, ULONG *> *);

	// inserts one SSimpleObject with key and value set to ulKey
	static ULLONG InsertOneElement(CCache<SSimpleObject *, ULONG *> *pCache,
								   ULONG ulKey);

	// inserts as many SSimpleObjects as needed (starting with the key ulKeyStart and
	// sequentially generating the successive keys) to consume cache quota.
	static ULONG ULFillCacheWithoutEviction(
		CCache<SSimpleObject *, ULONG *> *pCache, ULONG ulKeyStart);

	// checks if after eviction we have more entries from newer generation than the older generation
	static void CheckGenerationSanityAfterEviction(
		CCache<SSimpleObject *, ULONG *> *pCache, ULLONG ullOneElemSize,
		ULONG ulOldGenBeginKey, ULONG ulOldGenEndKey, ULONG ulNewGenEndKey);

	// tests if cache eviction works for a single cache size
	static void TestEvictionForOneCacheSize(ULLONG ullCacheQuota);


	// An object with a deep structure
	class CDeepObject : public CRefCount
	{
	private:
		// linked list entry
		struct SDeepObjectEntry
		{
			// a link to connect entries together
			SLink m_link;

			// entry's key
			ULONG m_ulKey;

			// entry's value
			ULONG m_ulValue;

			// ctor
			SDeepObjectEntry(ULONG ulKey, ULONG ulVal)
				: m_ulKey(ulKey), m_ulValue(ulVal)
			{
			}

		};	// struct SDeepObjectEntry

		// a deep structure given by a linked list
		CList<SDeepObjectEntry> m_list;

	public:
		typedef CList<SDeepObjectEntry> CDeepObjectList;

		// ctor
		CDeepObject()
		{
			m_list.Init(GPOS_OFFSET(SDeepObjectEntry, m_link));
		}

		// hashing  function
		static ULONG UlMyHash(CDeepObject::CDeepObjectList *const &plist);

		// key equality function
		static BOOL FMyEqual(CDeepObject::CDeepObjectList *const &pvKey,
							 CDeepObject::CDeepObjectList *const &pvKeySecond);

		// key accessor
		CDeepObjectList *
		Key()
		{
			return &m_list;
		}

		// add a new entry to the linked list
		void AddEntry(CMemoryPool *mp, ULONG ulKey, ULONG ulVal);

	};	// class CDeepObject


	// accessors type definitions
	typedef CCacheAccessor<SSimpleObject *, ULONG *> CSimpleObjectCacheAccessor;
	typedef CCacheAccessor<CDeepObject *, CDeepObject::CDeepObjectList *>
		CDeepObjectCacheAccessor;

	// cache task function pointer
	typedef void *(*TaskFuncPtr)(void *);

public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();
	static GPOS_RESULT EresUnittest_Refcount();
	static GPOS_RESULT EresUnittest_Eviction();
	static GPOS_RESULT EresUnittest_DeepObject();
	static GPOS_RESULT EresUnittest_Iteration();
	static GPOS_RESULT EresUnittest_IterativeDeletion();


};	// class CCacheTest

}  // namespace gpos

#endif	// GPOS_CCACHETEST_H_

// EOF

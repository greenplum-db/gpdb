//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CXformFactory.cpp
//
//	@doc:
//		Management of the global xform set
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/memory/CMemoryPoolManager.h"

#include "gpopt/xforms/xforms.h"

using namespace gpopt;

// global instance of xform factory
CXformFactory *CXformFactory::m_pxff = nullptr;


//---------------------------------------------------------------------------
//	@function:
//		CXformFactory::CXformFactory
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformFactory::CXformFactory(CMemoryPool *mp)
	: m_mp(mp),
	  m_phmszxform(nullptr),
	  m_pxfsExploration(nullptr),
	  m_pxfsImplementation(nullptr),
	  m_lastAddedOrSkippedXformId(-1)
{
	GPOS_ASSERT(nullptr != mp);

	// null out array so dtor can be called prematurely
	for (ULONG i = 0; i < CXform::ExfSentinel; i++)
	{
		m_rgpxf[i] = nullptr;
	}
	m_phmszxform = GPOS_NEW(mp) XformNameToXformMap(mp);
	m_pxfsExploration = GPOS_NEW(mp) CXformSet(mp);
	m_pxfsImplementation = GPOS_NEW(mp) CXformSet(mp);
}


//---------------------------------------------------------------------------
//	@function:
//		CXformFactory::~CXformFactory
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CXformFactory::~CXformFactory()
{
	GPOS_ASSERT(nullptr == m_pxff && "Xform factory has not been shut down");

	// delete all xforms in the array
	for (ULONG i = 0; i < CXform::ExfSentinel; i++)
	{
		if (nullptr == m_rgpxf[i])
		{
			// dtor called after failing to populate array
			break;
		}

		m_rgpxf[i]->Release();
		m_rgpxf[i] = nullptr;
	}

	m_phmszxform->Release();
	m_pxfsExploration->Release();
	m_pxfsImplementation->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CXformFactory::Add
//
//	@doc:
//		Add a given xform to the array; enforce the order in which they
//		are added for readability/debugging
//
//---------------------------------------------------------------------------
void
CXformFactory::Add(CXform *pxform)
{
	GPOS_ASSERT(nullptr != pxform);
	CXform::EXformId exfid = pxform->Exfid();

	GPOS_ASSERT(nullptr == m_rgpxf[exfid]);

	m_lastAddedOrSkippedXformId++;
	GPOS_ASSERT(exfid == m_lastAddedOrSkippedXformId &&
				"Incorrect order of instantiation");
	m_rgpxf[exfid] = pxform;

	// create name -> xform mapping
	ULONG length = clib::Strlen(pxform->SzId());
	CHAR *szXformName = GPOS_NEW_ARRAY(m_mp, CHAR, length + 1);
	clib::Strncpy(szXformName, pxform->SzId(), length + 1);

	BOOL fInserted GPOS_ASSERTS_ONLY =
		m_phmszxform->Insert(szXformName, pxform);
	GPOS_ASSERT(fInserted);

	CXformSet *xform_set = m_pxfsExploration;
	if (pxform->FImplementation())
	{
		xform_set = m_pxfsImplementation;
	}
#ifdef GPOS_DEBUG
	GPOS_ASSERT_IMP(pxform->FExploration(), xform_set == m_pxfsExploration);
	GPOS_ASSERT_IMP(pxform->FImplementation(),
					xform_set == m_pxfsImplementation);
	BOOL fSet =
#endif	// GPOS_DEBUG
		xform_set->ExchangeSet(exfid);

	GPOS_ASSERT(!fSet);
}


//---------------------------------------------------------------------------
//	@function:
//		CXformFactory::Instantiate
//
//	@doc:
//		Construct all xforms
//
//---------------------------------------------------------------------------
void
CXformFactory::Instantiate()
{
	// Order here needs to correspond to the order defined in CXform::EXformId
	Add(GPOS_NEW(m_mp) CXformProject2ComputeScalar(m_mp));
	Add(GPOS_NEW(m_mp) CXformExpandNAryJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformExpandNAryJoinMinCard(m_mp));
	Add(GPOS_NEW(m_mp) CXformExpandNAryJoinDP(m_mp));
	Add(GPOS_NEW(m_mp) CXformGet2TableScan(m_mp));
	Add(GPOS_NEW(m_mp) CXformIndexGet2IndexScan(m_mp));
	Add(GPOS_NEW(m_mp) CXformDynamicGet2DynamicTableScan(m_mp));
	Add(GPOS_NEW(m_mp) CXformDynamicIndexGet2DynamicIndexScan(m_mp));
	Add(GPOS_NEW(m_mp) CXformImplementSequence(m_mp));
	Add(GPOS_NEW(m_mp) CXformImplementConstTableGet(m_mp));
	Add(GPOS_NEW(m_mp) CXformUnnestTVF(m_mp));
	Add(GPOS_NEW(m_mp) CXformImplementTVF(m_mp));
	Add(GPOS_NEW(m_mp) CXformImplementTVFNoArgs(m_mp));
	Add(GPOS_NEW(m_mp) CXformSelect2Filter(m_mp));
	Add(GPOS_NEW(m_mp) CXformSelect2IndexGet(m_mp));
	Add(GPOS_NEW(m_mp) CXformSelect2DynamicIndexGet(m_mp));
	SkipUnused(1);
	Add(GPOS_NEW(m_mp) CXformSimplifySelectWithSubquery(m_mp));
	Add(GPOS_NEW(m_mp) CXformSimplifyProjectWithSubquery(m_mp));
	Add(GPOS_NEW(m_mp) CXformSelect2Apply(m_mp));
	Add(GPOS_NEW(m_mp) CXformProject2Apply(m_mp));
	Add(GPOS_NEW(m_mp) CXformGbAgg2Apply(m_mp));
	Add(GPOS_NEW(m_mp) CXformSubqJoin2Apply(m_mp));
	Add(GPOS_NEW(m_mp) CXformSubqNAryJoin2Apply(m_mp));
	SkipUnused(2);
	Add(GPOS_NEW(m_mp) CXformInnerApplyWithOuterKey2InnerJoin(m_mp));
	SkipUnused(1);
	Add(GPOS_NEW(m_mp) CXformImplementIndexApply(m_mp));
	SkipUnused(1);
	Add(GPOS_NEW(m_mp) CXformInnerApply2InnerJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformInnerApply2InnerJoinNoCorrelations(m_mp));
	Add(GPOS_NEW(m_mp) CXformImplementInnerCorrelatedApply(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftOuterApply2LeftOuterJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftOuterApply2LeftOuterJoinNoCorrelations(m_mp));
	Add(GPOS_NEW(m_mp) CXformImplementLeftOuterCorrelatedApply(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftSemiApply2LeftSemiJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftSemiApplyWithExternalCorrs2InnerJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftSemiApply2LeftSemiJoinNoCorrelations(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftAntiSemiApply2LeftAntiSemiJoin(m_mp));
	Add(GPOS_NEW(m_mp)
			CXformLeftAntiSemiApply2LeftAntiSemiJoinNoCorrelations(m_mp));
	Add(GPOS_NEW(m_mp)
			CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotIn(m_mp));
	Add(GPOS_NEW(m_mp)
			CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations(
				m_mp));
	Add(GPOS_NEW(m_mp) CXformPushDownLeftOuterJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformSimplifyLeftOuterJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftOuterJoin2NLJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftOuterJoin2HashJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftSemiJoin2NLJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftSemiJoin2HashJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftAntiSemiJoin2CrossProduct(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftAntiSemiJoinNotIn2CrossProduct(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftAntiSemiJoin2NLJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftAntiSemiJoinNotIn2NLJoinNotIn(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftAntiSemiJoin2HashJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftAntiSemiJoinNotIn2HashJoinNotIn(m_mp));
	Add(GPOS_NEW(m_mp) CXformGbAgg2HashAgg(m_mp));
	Add(GPOS_NEW(m_mp) CXformGbAgg2StreamAgg(m_mp));
	Add(GPOS_NEW(m_mp) CXformGbAgg2ScalarAgg(m_mp));
	Add(GPOS_NEW(m_mp) CXformGbAggDedup2HashAggDedup(m_mp));
	Add(GPOS_NEW(m_mp) CXformGbAggDedup2StreamAggDedup(m_mp));
	Add(GPOS_NEW(m_mp) CXformImplementLimit(m_mp));
	Add(GPOS_NEW(m_mp) CXformIntersectAll2LeftSemiJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformIntersect2Join(m_mp));
	Add(GPOS_NEW(m_mp) CXformDifference2LeftAntiSemiJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformDifferenceAll2LeftAntiSemiJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformUnion2UnionAll(m_mp));
	Add(GPOS_NEW(m_mp) CXformImplementUnionAll(m_mp));
	Add(GPOS_NEW(m_mp) CXformInsert2DML(m_mp));
	Add(GPOS_NEW(m_mp) CXformDelete2DML(m_mp));
	Add(GPOS_NEW(m_mp) CXformUpdate2DML(m_mp));
	Add(GPOS_NEW(m_mp) CXformImplementDML(m_mp));
	SkipUnused(1);
	Add(GPOS_NEW(m_mp) CXformImplementSplit(m_mp));
	Add(GPOS_NEW(m_mp) CXformJoinCommutativity(m_mp));
	Add(GPOS_NEW(m_mp) CXformJoinAssociativity(m_mp));
	Add(GPOS_NEW(m_mp) CXformSemiJoinSemiJoinSwap(m_mp));
	Add(GPOS_NEW(m_mp) CXformSemiJoinAntiSemiJoinSwap(m_mp));
	Add(GPOS_NEW(m_mp) CXformSemiJoinAntiSemiJoinNotInSwap(m_mp));
	Add(GPOS_NEW(m_mp) CXformSemiJoinInnerJoinSwap(m_mp));
	Add(GPOS_NEW(m_mp) CXformAntiSemiJoinAntiSemiJoinSwap(m_mp));
	Add(GPOS_NEW(m_mp) CXformAntiSemiJoinAntiSemiJoinNotInSwap(m_mp));
	Add(GPOS_NEW(m_mp) CXformAntiSemiJoinSemiJoinSwap(m_mp));
	Add(GPOS_NEW(m_mp) CXformAntiSemiJoinInnerJoinSwap(m_mp));
	Add(GPOS_NEW(m_mp) CXformAntiSemiJoinNotInAntiSemiJoinSwap(m_mp));
	Add(GPOS_NEW(m_mp) CXformAntiSemiJoinNotInAntiSemiJoinNotInSwap(m_mp));
	Add(GPOS_NEW(m_mp) CXformAntiSemiJoinNotInSemiJoinSwap(m_mp));
	Add(GPOS_NEW(m_mp) CXformAntiSemiJoinNotInInnerJoinSwap(m_mp));
	Add(GPOS_NEW(m_mp) CXformInnerJoinSemiJoinSwap(m_mp));
	Add(GPOS_NEW(m_mp) CXformInnerJoinAntiSemiJoinSwap(m_mp));
	Add(GPOS_NEW(m_mp) CXformInnerJoinAntiSemiJoinNotInSwap(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftSemiJoin2InnerJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftSemiJoin2InnerJoinUnderGb(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftSemiJoin2CrossProduct(m_mp));
	Add(GPOS_NEW(m_mp) CXformSplitLimit(m_mp));
	Add(GPOS_NEW(m_mp) CXformSimplifyGbAgg(m_mp));
	Add(GPOS_NEW(m_mp) CXformCollapseGbAgg(m_mp));
	Add(GPOS_NEW(m_mp) CXformPushGbBelowJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformPushGbDedupBelowJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformPushGbWithHavingBelowJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformPushGbBelowUnion(m_mp));
	Add(GPOS_NEW(m_mp) CXformPushGbBelowUnionAll(m_mp));
	Add(GPOS_NEW(m_mp) CXformSplitGbAgg(m_mp));
	Add(GPOS_NEW(m_mp) CXformSplitGbAggDedup(m_mp));
	Add(GPOS_NEW(m_mp) CXformSplitDQA(m_mp));
	Add(GPOS_NEW(m_mp) CXformSequenceProject2Apply(m_mp));
	Add(GPOS_NEW(m_mp) CXformImplementSequenceProject(m_mp));
	Add(GPOS_NEW(m_mp) CXformImplementAssert(m_mp));
	Add(GPOS_NEW(m_mp) CXformCTEAnchor2Sequence(m_mp));
	Add(GPOS_NEW(m_mp) CXformCTEAnchor2TrivialSelect(m_mp));
	Add(GPOS_NEW(m_mp) CXformInlineCTEConsumer(m_mp));
	Add(GPOS_NEW(m_mp) CXformInlineCTEConsumerUnderSelect(m_mp));
	Add(GPOS_NEW(m_mp) CXformImplementCTEProducer(m_mp));
	Add(GPOS_NEW(m_mp) CXformImplementCTEConsumer(m_mp));
	Add(GPOS_NEW(m_mp) CXformExpandFullOuterJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformForeignGet2ForeignScan(m_mp));
	Add(GPOS_NEW(m_mp) CXformSelect2BitmapBoolOp(m_mp));
	Add(GPOS_NEW(m_mp) CXformSelect2DynamicBitmapBoolOp(m_mp));
	Add(GPOS_NEW(m_mp) CXformImplementBitmapTableGet(m_mp));
	Add(GPOS_NEW(m_mp) CXformImplementDynamicBitmapTableGet(m_mp));
	SkipUnused(1);
	Add(GPOS_NEW(m_mp) CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformImplementLeftSemiCorrelatedApply(m_mp));
	Add(GPOS_NEW(m_mp) CXformImplementLeftSemiCorrelatedApplyIn(m_mp));
	Add(GPOS_NEW(m_mp) CXformImplementLeftAntiSemiCorrelatedApply(m_mp));
	Add(GPOS_NEW(m_mp) CXformImplementLeftAntiSemiCorrelatedApplyNotIn(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftSemiApplyIn2LeftSemiJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftSemiApplyInWithExternalCorrs2InnerJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformLeftSemiApplyIn2LeftSemiJoinNoCorrelations(m_mp));
	SkipUnused(1);
	Add(GPOS_NEW(m_mp) CXformImplementPartitionSelector(m_mp));
	Add(GPOS_NEW(m_mp) CXformMaxOneRow2Assert(m_mp));
	SkipUnused(6);
	Add(GPOS_NEW(m_mp) CXformGbAggWithMDQA2Join(m_mp));
	Add(GPOS_NEW(m_mp) CXformCollapseProject(m_mp));
	Add(GPOS_NEW(m_mp) CXformRemoveSubqDistinct(m_mp));
	SkipUnused(4);
	Add(GPOS_NEW(m_mp) CXformExpandNAryJoinGreedy(m_mp));
	Add(GPOS_NEW(m_mp) CXformEagerAgg(m_mp));
	Add(GPOS_NEW(m_mp) CXformExpandNAryJoinDPv2(m_mp));
	Add(GPOS_NEW(m_mp) CXformImplementFullOuterMergeJoin(m_mp));
	SkipUnused(4);
	Add(GPOS_NEW(m_mp) CXformIndexGet2IndexOnlyScan(m_mp));
	Add(GPOS_NEW(m_mp) CXformJoin2BitmapIndexGetApply(m_mp));
	Add(GPOS_NEW(m_mp) CXformJoin2IndexGetApply(m_mp));
	SkipUnused(2);
	Add(GPOS_NEW(m_mp) CXformLeftJoin2RightJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformRightOuterJoin2HashJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformImplementInnerJoin(m_mp));
	Add(GPOS_NEW(m_mp) CXformDynamicForeignGet2DynamicForeignScan(m_mp));
	Add(GPOS_NEW(m_mp) CXformExpandDynamicGetWithForeignPartitions(m_mp));

	GPOS_ASSERT(nullptr != m_rgpxf[CXform::ExfSentinel - 1] &&
				"Not all xforms have been instantiated");
}


//---------------------------------------------------------------------------
//	@function:
//		CXformFactory::Pxf
//
//	@doc:
//		Accessor of xform array
//
//---------------------------------------------------------------------------
CXform *
CXformFactory::Pxf(CXform::EXformId exfid) const
{
	CXform *pxf = m_rgpxf[exfid];
	GPOS_ASSERT(pxf->Exfid() == exfid);

	return pxf;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformFactory::Pxf
//
//	@doc:
//		Accessor by xform name
//
//---------------------------------------------------------------------------
CXform *
CXformFactory::Pxf(const CHAR *szXformName) const
{
	return m_phmszxform->Find(szXformName);
}


// is this xform id still used?
BOOL
CXformFactory::IsXformIdUsed(CXform::EXformId exfid)
{
	GPOS_ASSERT(exfid <= m_lastAddedOrSkippedXformId);

	return (nullptr != m_rgpxf[exfid]);
}


//---------------------------------------------------------------------------
//	@function:
//		CXformFactory::Init
//
//	@doc:
//		Initializes global instance
//
//---------------------------------------------------------------------------
GPOS_RESULT
CXformFactory::Init()
{
	GPOS_ASSERT(nullptr == Pxff() && "Xform factory was already initialized");

	GPOS_RESULT eres = GPOS_OK;

	// create xform factory memory pool
	CMemoryPool *mp =
		CMemoryPoolManager::GetMemoryPoolMgr()->CreateMemoryPool();
	GPOS_TRY
	{
		// create xform factory instance
		m_pxff = GPOS_NEW(mp) CXformFactory(mp);
	}
	GPOS_CATCH_EX(ex)
	{
		// destroy memory pool if global instance was not created
		CMemoryPoolManager::GetMemoryPoolMgr()->Destroy(mp);
		m_pxff = nullptr;

		if (GPOS_MATCH_EX(ex, CException::ExmaSystem, CException::ExmiOOM))
		{
			eres = GPOS_OOM;
		}
		else
		{
			eres = GPOS_FAILED;
		}

		return eres;
	}
	GPOS_CATCH_END;

	// instantiating the factory
	m_pxff->Instantiate();

	return eres;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformFactory::Shutdown
//
//	@doc:
//		Cleans up allocated memory pool
//
//---------------------------------------------------------------------------
void
CXformFactory::Shutdown()
{
	CXformFactory *pxff = CXformFactory::Pxff();

	GPOS_ASSERT(nullptr != pxff && "Xform factory has not been initialized");

	CMemoryPool *mp = pxff->m_mp;

	// destroy xform factory
	CXformFactory::m_pxff = nullptr;
	GPOS_DELETE(pxff);

	// release allocated memory pool
	CMemoryPoolManager::GetMemoryPoolMgr()->Destroy(mp);
}


// EOF

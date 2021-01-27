//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CMDAccessorTest.cpp
//
//	@doc:
//		Tests accessing objects from the metadata cache.
//---------------------------------------------------------------------------

#include "unittest/gpopt/mdcache/CMDAccessorTest.h"

#include "gpos/error/CAutoTrace.h"
#include "gpos/io/COstreamString.h"
#include "gpos/memory/CCacheFactory.h"
#include "gpos/string/CWStringDynamic.h"
#include "gpos/task/CAutoTaskProxy.h"

#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "naucrates/base/IDatumBool.h"
#include "naucrates/base/IDatumInt4.h"
#include "naucrates/base/IDatumOid.h"
#include "naucrates/exception.h"
#include "naucrates/md/CMDIdGPDB.h"
#include "naucrates/md/CMDProviderMemory.h"
#include "naucrates/md/IMDAggregate.h"
#include "naucrates/md/IMDCast.h"
#include "naucrates/md/IMDCheckConstraint.h"
#include "naucrates/md/IMDColumn.h"
#include "naucrates/md/IMDFunction.h"
#include "naucrates/md/IMDIndex.h"
#include "naucrates/md/IMDPartConstraint.h"
#include "naucrates/md/IMDRelation.h"
#include "naucrates/md/IMDScCmp.h"
#include "naucrates/md/IMDScalarOp.h"
#include "naucrates/md/IMDTypeBool.h"
#include "naucrates/md/IMDTypeGeneric.h"
#include "naucrates/md/IMDTypeInt4.h"
#include "naucrates/md/IMDTypeOid.h"

#include "unittest/base.h"
#include "unittest/gpopt/CTestUtils.h"
#include "unittest/gpopt/mdcache/CMDProviderTest.h"


#define GPOPT_MDCACHE_LOOKUP_THREADS 8
#define GPOPT_MDCACHE_MDAS 4

#define GPDB_AGG_AVG OID(2101)
#define GPDB_OP_INT4_LT OID(97)
#define GPDB_FUNC_TIMEOFDAY OID(274)

//---------------------------------------------------------------------------
//	@function:
//		CMDAccessorTest::EresUnittest
//
//	@doc:
//
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMDAccessorTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CMDAccessorTest::EresUnittest_Basic),
		GPOS_UNITTEST_FUNC(CMDAccessorTest::EresUnittest_Datum),
#ifdef GPOS_DEBUG
		GPOS_UNITTEST_FUNC_ASSERT(CMDAccessorTest::EresUnittest_DatumGeneric),
#endif
		GPOS_UNITTEST_FUNC(CMDAccessorTest::EresUnittest_Navigate),
		GPOS_UNITTEST_FUNC_THROW(CMDAccessorTest::EresUnittest_Negative,
								 gpdxl::ExmaMD,
								 gpdxl::ExmiMDCacheEntryNotFound),
		GPOS_UNITTEST_FUNC(CMDAccessorTest::EresUnittest_Indexes),
		GPOS_UNITTEST_FUNC(CMDAccessorTest::EresUnittest_CheckConstraint),
		GPOS_UNITTEST_FUNC(CMDAccessorTest::EresUnittest_IndexPartConstraint),
		GPOS_UNITTEST_FUNC(CMDAccessorTest::EresUnittest_Cast),
		GPOS_UNITTEST_FUNC(CMDAccessorTest::EresUnittest_ScCmp)};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CMDAccessorTest::EresUnittest_Basic
//
//	@doc:
//		Test fetching metadata objects from a metadata cache
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMDAccessorTest::EresUnittest_Basic()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
					 CTestUtils::GetCostModel(mp));

	// lookup different objects
	CMDIdGPDB *pmdidObject1 =
		GPOS_NEW(mp) CMDIdGPDB(GPOPT_MDCACHE_TEST_OID /* OID */,
							   1 /* major version */, 1 /* minor version */);
	CMDIdGPDB *pmdidObject2 =
		GPOS_NEW(mp) CMDIdGPDB(GPOPT_MDCACHE_TEST_OID /* OID */,
							   12 /* version */, 1 /* minor version */);

#ifdef GPOS_DEBUG
	const IMDRelation *pimdrel1 =
#endif
		mda.RetrieveRel(pmdidObject1);

#ifdef GPOS_DEBUG
	const IMDRelation *pimdrel2 =
#endif
		mda.RetrieveRel(pmdidObject2);

	GPOS_ASSERT(pimdrel1->MDId()->Equals(pmdidObject1) &&
				pimdrel2->MDId()->Equals(pmdidObject2));

	// access an object again
#ifdef GPOS_DEBUG
	const IMDRelation *pimdrel3 =
#endif
		mda.RetrieveRel(pmdidObject1);

	GPOS_ASSERT(pimdrel1 == pimdrel3);

	// access GPDB types, operators and aggregates
	CMDIdGPDB *mdid_type = GPOS_NEW(mp) CMDIdGPDB(GPDB_INT4, 1, 0);
	CMDIdGPDB *mdid_op = GPOS_NEW(mp) CMDIdGPDB(GPDB_OP_INT4_LT, 1, 0);
	CMDIdGPDB *agg_mdid = GPOS_NEW(mp) CMDIdGPDB(GPDB_AGG_AVG, 1, 0);

#ifdef GPOS_DEBUG
	const IMDType *pimdtype =
#endif
		mda.RetrieveType(mdid_type);

#ifdef GPOS_DEBUG
	const IMDScalarOp *md_scalar_op =
#endif
		mda.RetrieveScOp(mdid_op);

	GPOS_ASSERT(IMDType::EcmptL == md_scalar_op->ParseCmpType());

#ifdef GPOS_DEBUG
	const IMDAggregate *pmdagg =
#endif
		mda.RetrieveAgg(agg_mdid);


	// access types by type info
#ifdef GPOS_DEBUG
	const IMDTypeInt4 *pmdtypeint4 =
#endif
		mda.PtMDType<IMDTypeInt4>(CTestUtils::m_sysidDefault);

#ifdef GPOS_DEBUG
	const IMDTypeBool *pmdtypebool =
#endif
		mda.PtMDType<IMDTypeBool>(CTestUtils::m_sysidDefault);


#ifdef GPOS_DEBUG
	// for debug traces
	CAutoTrace at(mp);
	IOstream &os(at.Os());

	// print objects
	os << std::endl;
	pimdrel1->DebugPrint(os);
	os << std::endl;

	pimdrel2->DebugPrint(os);
	os << std::endl;

	pimdtype->DebugPrint(os);
	os << std::endl;

	md_scalar_op->DebugPrint(os);
	os << std::endl;

	pmdagg->DebugPrint(os);
	os << std::endl;

	pmdtypeint4->DebugPrint(os);
	os << std::endl;

	pmdtypebool->DebugPrint(os);
	os << std::endl;

#endif

	pmdidObject1->Release();
	pmdidObject2->Release();
	agg_mdid->Release();
	mdid_op->Release();
	mdid_type->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDAccessorTest::EresUnittest_Datum
//
//	@doc:
//		Test type factory method for creating base type datums
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMDAccessorTest::EresUnittest_Datum()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
					 CTestUtils::GetCostModel(mp));

	// create an INT4 datum
	const IMDTypeInt4 *pmdtypeint4 =
		mda.PtMDType<IMDTypeInt4>(CTestUtils::m_sysidDefault);
	IDatumInt4 *pdatumInt4 =
		pmdtypeint4->CreateInt4Datum(mp, 5, false /* is_null */);
	GPOS_ASSERT(5 == pdatumInt4->Value());

	// create a BOOL datum
	const IMDTypeBool *pmdtypebool =
		mda.PtMDType<IMDTypeBool>(CTestUtils::m_sysidDefault);
	IDatumBool *pdatumBool =
		pmdtypebool->CreateBoolDatum(mp, false, false /* is_null */);
	GPOS_ASSERT(false == pdatumBool->GetValue());

	// create an OID datum
	const IMDTypeOid *pmdtypeoid =
		mda.PtMDType<IMDTypeOid>(CTestUtils::m_sysidDefault);
	IDatumOid *pdatumOid =
		pmdtypeoid->CreateOidDatum(mp, 20, false /* is_null */);
	GPOS_ASSERT(20 == pdatumOid->OidValue());


#ifdef GPOS_DEBUG
	// for debug traces
	CWStringDynamic str(mp);
	COstreamString oss(&str);

	// print objects
	oss << std::endl;
	oss << "Int4 datum" << std::endl;
	pdatumInt4->OsPrint(oss);

	oss << std::endl;
	oss << "Bool datum" << std::endl;
	pdatumBool->OsPrint(oss);

	oss << std::endl;
	oss << "Oid datum" << std::endl;
	pdatumOid->OsPrint(oss);

	GPOS_TRACE(str.GetBuffer());
#endif

	// cleanup
	pdatumInt4->Release();
	pdatumBool->Release();
	pdatumOid->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDAccessorTest::EresUnittest_DatumGeneric
//
//	@doc:
//		Test asserting during an attempt to obtain a generic type from the cache
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMDAccessorTest::EresUnittest_DatumGeneric()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// Setup an MD cache with a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// attempt to obtain a generic type from the cache should assert
	(void) mda.PtMDType<IMDTypeGeneric>();

	return GPOS_FAILED;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDAccessorTest::EresUnittest_Navigate
//
//	@doc:
//		Test fetching a MD object from the cache and navigating its dependent
//		objects
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMDAccessorTest::EresUnittest_Navigate()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();

	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// lookup a function in the MD cache
	CMDIdGPDB *mdid_func =
		GPOS_NEW(mp) CMDIdGPDB(GPDB_FUNC_TIMEOFDAY /* OID */,
							   1 /* major version */, 0 /* minor version */);

	const IMDFunction *pmdfunc = mda.RetrieveFunc(mdid_func);

	// lookup function return type
	IMDId *pmdidFuncReturnType = pmdfunc->GetResultTypeMdid();
	const IMDType *pimdtype = mda.RetrieveType(pmdidFuncReturnType);

	// lookup equality operator for function return type
	IMDId *pmdidEqOp = pimdtype->GetMdidForCmpType(IMDType::EcmptEq);

#ifdef GPOS_DEBUG
	const IMDScalarOp *md_scalar_op =
#endif
		mda.RetrieveScOp(pmdidEqOp);

#ifdef GPOS_DEBUG
	// print objects
	CWStringDynamic str(mp);
	COstreamString oss(&str);

	oss << std::endl;
	oss << std::endl;

	pmdfunc->DebugPrint(oss);
	oss << std::endl;

	pimdtype->DebugPrint(oss);
	oss << std::endl;

	md_scalar_op->DebugPrint(oss);
	oss << std::endl;

	GPOS_TRACE(str.GetBuffer());

#endif

	mdid_func->Release();

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CMDAccessorTest::EresUnittest_Indexes
//
//	@doc:
//		Test fetching indexes from the cache
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMDAccessorTest::EresUnittest_Indexes()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CAutoTrace at(mp);

#ifdef GPOS_DEBUG
	IOstream &os(at.Os());
#endif	// GPOS_DEBUG

	// Setup an MD cache with a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
					 CTestUtils::GetCostModel(mp));

	// lookup a relation in the MD cache
	CMDIdGPDB *rel_mdid = GPOS_NEW(mp)
		CMDIdGPDB(GPOPT_MDCACHE_TEST_OID, 1 /* major */, 1 /* minor version */);

	const IMDRelation *pmdrel = mda.RetrieveRel(rel_mdid);

	GPOS_ASSERT(0 < pmdrel->IndexCount());

	IMDId *pmdidIndex = pmdrel->IndexMDidAt(0);
	const IMDIndex *pmdindex = mda.RetrieveIndex(pmdidIndex);

	ULONG ulKeys = pmdindex->Keys();

#ifdef GPOS_DEBUG
	// print index
	pmdindex->DebugPrint(os);

	os << std::endl;
	os << "Index columns: " << std::endl;

#endif	// GPOS_DEBUG

	for (ULONG ul = 0; ul < ulKeys; ul++)
	{
		ULONG ulKeyColumn = pmdindex->KeyAt(ul);

#ifdef GPOS_DEBUG
		const IMDColumn *pmdcol =
#endif	// GPOS_DEBUG
			pmdrel->GetMdCol(ulKeyColumn);

#ifdef GPOS_DEBUG
		pmdcol->DebugPrint(os);
#endif	// GPOS_DEBUG
	}

	rel_mdid->Release();
	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDAccessorTest::EresUnittest_CheckConstraint
//
//	@doc:
//		Test fetching check constraint from the cache and translating the
//		check constraint scalar expression
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMDAccessorTest::EresUnittest_CheckConstraint()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CAutoTrace at(mp);

#ifdef GPOS_DEBUG
	IOstream &os(at.Os());
#endif	// GPOS_DEBUG

	// Setup an MD cache with a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
					 CTestUtils::GetCostModel(mp));
	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();

	// lookup a relation in the MD cache
	CMDIdGPDB *rel_mdid = GPOS_NEW(mp)
		CMDIdGPDB(GPOPT_TEST_REL_OID21, 1 /* major */, 1 /* minor version */);

	const IMDRelation *pmdrel = mda.RetrieveRel(rel_mdid);
	GPOS_ASSERT(0 < pmdrel->CheckConstraintCount());

	// create the array of column reference for the table columns
	// for the DXL to Expr translation
	CColRefArray *colref_array = GPOS_NEW(mp) CColRefArray(mp);
	const ULONG num_cols = pmdrel->ColumnCount() - pmdrel->SystemColumnsCount();
	for (ULONG ul = 0; ul < num_cols; ul++)
	{
		const IMDColumn *pmdcol = pmdrel->GetMdCol(ul);
		const IMDType *pmdtype = mda.RetrieveType(pmdcol->MdidType());
		CColRef *colref =
			col_factory->PcrCreate(pmdtype, pmdcol->TypeModifier());
		colref_array->Append(colref);
	}

	// get one of its check constraint
	IMDId *pmdidCheckConstraint = pmdrel->CheckConstraintMDidAt(0);
	const IMDCheckConstraint *pmdCheckConstraint =
		mda.RetrieveCheckConstraints(pmdidCheckConstraint);

#ifdef GPOS_DEBUG
	os << std::endl;
	// print check constraint
	pmdCheckConstraint->DebugPrint(os);
	os << std::endl;
#endif	// GPOS_DEBUG

	// extract and then print the check constraint expression
	CExpression *pexpr =
		pmdCheckConstraint->GetCheckConstraintExpr(mp, &mda, colref_array);

#ifdef GPOS_DEBUG
	pexpr->DbgPrint();
	os << std::endl;
#endif	// GPOS_DEBUG

	// clean up
	pexpr->Release();
	colref_array->Release();
	rel_mdid->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDAccessorTest::EresUnittest_IndexPartConstraint
//
//	@doc:
//		Test fetching part constraints for indexes on partitioned tables
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMDAccessorTest::EresUnittest_IndexPartConstraint()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CAutoTrace at(mp);

	// Setup an MD cache with a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
					 CTestUtils::GetCostModel(mp));
	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();

	// lookup a relation in the MD cache
	CMDIdGPDB *rel_mdid = GPOS_NEW(mp) CMDIdGPDB(GPOPT_TEST_REL_OID22);

	const IMDRelation *pmdrel = mda.RetrieveRel(rel_mdid);
	GPOS_ASSERT(0 < pmdrel->IndexCount());

	// create the array of column reference for the table columns
	// for the DXL to Expr translation
	CColRefArray *colref_array = GPOS_NEW(mp) CColRefArray(mp);
	const ULONG num_cols = pmdrel->ColumnCount() - pmdrel->SystemColumnsCount();
	for (ULONG ul = 0; ul < num_cols; ul++)
	{
		const IMDColumn *pmdcol = pmdrel->GetMdCol(ul);
		const IMDType *pmdtype = mda.RetrieveType(pmdcol->MdidType());
		CColRef *colref =
			col_factory->PcrCreate(pmdtype, pmdcol->TypeModifier());
		colref_array->Append(colref);
	}

	// get one of its indexes
	GPOS_ASSERT(0 < pmdrel->IndexCount());
	IMDId *pmdidIndex = pmdrel->IndexMDidAt(0);
	const IMDIndex *pmdindex = mda.RetrieveIndex(pmdidIndex);

	// extract and then print the part constraint expression
	IMDPartConstraint *mdpart_constraint = pmdindex->MDPartConstraint();
	GPOS_ASSERT(NULL != mdpart_constraint);

	CExpression *pexpr =
		mdpart_constraint->GetPartConstraintExpr(mp, &mda, colref_array);

#ifdef GPOS_DEBUG
	IOstream &os(at.Os());

	pexpr->DbgPrint();
	os << std::endl;
#endif	// GPOS_DEBUG

	// clean up
	pexpr->Release();
	colref_array->Release();
	rel_mdid->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDAccessorTest::EresUnittest_Cast
//
//	@doc:
//		Test fetching cast objects from the MD cache
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMDAccessorTest::EresUnittest_Cast()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CAutoTrace at(mp);

	// Setup an MD cache with a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
					 CTestUtils::GetCostModel(mp));

	const IMDType *pmdtypeInt =
		mda.PtMDType<IMDTypeInt4>(CTestUtils::m_sysidDefault);
	const IMDType *pmdtypeOid =
		mda.PtMDType<IMDTypeOid>(CTestUtils::m_sysidDefault);
	const IMDType *pmdtypeBigInt =
		mda.PtMDType<IMDTypeInt8>(CTestUtils::m_sysidDefault);

#ifdef GPOS_DEBUG
	const IMDCast *pmdcastInt2BigInt =
#endif	// GPOS_DEBUG
		mda.Pmdcast(pmdtypeInt->MDId(), pmdtypeBigInt->MDId());

	GPOS_ASSERT(!pmdcastInt2BigInt->IsBinaryCoercible());
	GPOS_ASSERT(pmdcastInt2BigInt->GetCastFuncMdId()->IsValid());
	GPOS_ASSERT(pmdcastInt2BigInt->MdidSrc()->Equals(pmdtypeInt->MDId()));
	GPOS_ASSERT(pmdcastInt2BigInt->MdidDest()->Equals(pmdtypeBigInt->MDId()));

#ifdef GPOS_DEBUG
	const IMDCast *pmdcastInt2Oid =
#endif	// GPOS_DEBUG
		mda.Pmdcast(pmdtypeInt->MDId(), pmdtypeOid->MDId());

	GPOS_ASSERT(pmdcastInt2Oid->IsBinaryCoercible());
	GPOS_ASSERT(!pmdcastInt2Oid->GetCastFuncMdId()->IsValid());

#ifdef GPOS_DEBUG
	const IMDCast *pmdcastOid2Int =
#endif	// GPOS_DEBUG
		mda.Pmdcast(pmdtypeOid->MDId(), pmdtypeInt->MDId());

	GPOS_ASSERT(pmdcastOid2Int->IsBinaryCoercible());
	GPOS_ASSERT(!pmdcastOid2Int->GetCastFuncMdId()->IsValid());

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDAccessorTest::EresUnittest_ScCmp
//
//	@doc:
//		Test fetching scalar comparison objects from the MD cache
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMDAccessorTest::EresUnittest_ScCmp()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CAutoTrace at(mp);

	// Setup an MD cache with a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
					 CTestUtils::GetCostModel(mp));

	const IMDType *pmdtypeInt =
		mda.PtMDType<IMDTypeInt4>(CTestUtils::m_sysidDefault);
	const IMDType *pmdtypeBigInt =
		mda.PtMDType<IMDTypeInt8>(CTestUtils::m_sysidDefault);

#ifdef GPOS_DEBUG
	const IMDScCmp *pmdScEqIntBigInt =
#endif	// GPOS_DEBUG
		mda.Pmdsccmp(pmdtypeInt->MDId(), pmdtypeBigInt->MDId(),
					 IMDType::EcmptEq);

	GPOS_ASSERT(IMDType::EcmptEq == pmdScEqIntBigInt->ParseCmpType());
	GPOS_ASSERT(pmdScEqIntBigInt->GetLeftMdid()->Equals(pmdtypeInt->MDId()));
	GPOS_ASSERT(
		pmdScEqIntBigInt->GetRightMdid()->Equals(pmdtypeBigInt->MDId()));

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDAccessorTest::EresUnittest_Negative
//
//	@doc:
//		Test fetching non-existing metadata objects from the MD cache
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMDAccessorTest::EresUnittest_Negative()
{
	CAutoMemoryPool amp(CAutoMemoryPool::ElcNone);
	CMemoryPool *mp = amp.Pmp();

	// Setup an MD cache with a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// lookup a non-existing objects
	CMDIdGPDB *pmdidNonExistingObject =
		GPOS_NEW(mp) CMDIdGPDB(GPOPT_MDCACHE_TEST_OID /* OID */,
							   15 /* version */, 1 /* minor version */);

	// call should result in an exception
	(void) mda.RetrieveRel(pmdidNonExistingObject);

	pmdidNonExistingObject->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDAccessorTest::PvLookupSingleObj
//
//	@doc:
//		A task that looks up a single object from the MD cache
//
//---------------------------------------------------------------------------
void *
CMDAccessorTest::PvLookupSingleObj(void *pv)
{
	GPOS_CHECK_ABORT;

	GPOS_ASSERT(NULL != pv);

	SMDCacheTaskParams *pmdtaskparams = (SMDCacheTaskParams *) pv;

	CMDAccessor *md_accessor = pmdtaskparams->m_pmda;

	CMemoryPool *mp = pmdtaskparams->m_mp;

	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(NULL != md_accessor);

	// lookup a cache object
	CMDIdGPDB *mdid =
		GPOS_NEW(mp) CMDIdGPDB(GPOPT_MDCACHE_TEST_OID /* OID */,
							   1 /* major version */, 1 /* minor version */);

	// lookup object
	(void) md_accessor->RetrieveRel(mdid);
	mdid->Release();

	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDAccessorTest::PvLookupMultipleObj
//
//	@doc:
//		A task that looks up multiple objects from the MD cache
//
//---------------------------------------------------------------------------
void *
CMDAccessorTest::PvLookupMultipleObj(void *pv)
{
	GPOS_CHECK_ABORT;

	GPOS_ASSERT(NULL != pv);

	SMDCacheTaskParams *pmdtaskparams = (SMDCacheTaskParams *) pv;

	CMDAccessor *md_accessor = pmdtaskparams->m_pmda;

	GPOS_ASSERT(NULL != md_accessor);

	// lookup cache objects
	const ULONG ulNumberOfObjects = 10;

	for (ULONG ul = 0; ul < ulNumberOfObjects; ul++)
	{
		GPOS_CHECK_ABORT;

		// lookup relation
		CMDIdGPDB *mdid = GPOS_NEW(pmdtaskparams->m_mp) CMDIdGPDB(
			GPOPT_MDCACHE_TEST_OID /*OID*/, 1 /*major*/, ul + 1 /*minor*/);
		(void) md_accessor->RetrieveRel(mdid);
		mdid->Release();
	}

	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDAccessorTest::PvInitMDAAndLookup
//
//	@doc:
//		Task which initializes a MD accessor and looks up multiple objects through
//		that accessor
//
//---------------------------------------------------------------------------
void *
CMDAccessorTest::PvInitMDAAndLookup(void *pv)
{
	GPOS_ASSERT(NULL != pv);

	CMDAccessor::MDCache *pcache = (CMDAccessor::MDCache *) pv;

	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();
	CWorkerPoolManager *pwpm = CWorkerPoolManager::WorkerPoolManager();

	// task memory pool
	CAutoMemoryPool ampTask;
	CMemoryPool *pmpTask = ampTask.Pmp();

	// scope for MD accessor
	{
		// Setup an MD cache with a file-based provider
		CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
		pmdp->AddRef();
		CMDAccessor mda(mp, pcache, CTestUtils::m_sysidDefault, pmdp);

		// task parameters
		SMDCacheTaskParams mdtaskparams(pmpTask, &mda);

		// scope for ATP
		{
			CAutoTaskProxy atp(mp, pwpm);

			CTask *rgPtsk[GPOPT_MDCACHE_LOOKUP_THREADS];

			TaskFuncPtr rgPfuncTask[] = {CMDAccessorTest::PvLookupSingleObj,
										 CMDAccessorTest::PvLookupMultipleObj};

			const ULONG ulNumberOfTaskTypes = GPOS_ARRAY_SIZE(rgPfuncTask);

			// create tasks
			for (ULONG i = 0; i < GPOS_ARRAY_SIZE(rgPtsk); i++)
			{
				rgPtsk[i] = atp.Create(rgPfuncTask[i % ulNumberOfTaskTypes],
									   &mdtaskparams);

				GPOS_CHECK_ABORT;
			}

			for (ULONG i = 0; i < GPOS_ARRAY_SIZE(rgPtsk); i++)
			{
				atp.Schedule(rgPtsk[i]);
			}

			GPOS_CHECK_ABORT;

			// wait for completion
			for (ULONG i = 0; i < GPOS_ARRAY_SIZE(rgPtsk); i++)
			{
				atp.Execute(rgPtsk[i]);
				GPOS_CHECK_ABORT;
			}
		}
	}

	return NULL;
}

// EOF

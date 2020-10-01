//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CTranslatorDXLToExprTest.cpp
//
//	@doc:
//		Tests translating DXL trees into Expr tree.
//
//---------------------------------------------------------------------------
#include "gpos/error/CException.h"
#include "gpos/error/CMessage.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/test/CUnittest.h"

#include "unittest/base.h"
#include "unittest/gpopt/translate/CTranslatorDXLToExprTest.h"

// XSD location
#include "unittest/dxl/CParseHandlerTest.h"
#include "unittest/gpopt/CTestUtils.h"

#include "naucrates/base/IDatum.h"
#include "naucrates/base/CQueryToDXLResult.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/md/IMDTypeBool.h"
#include "naucrates/md/IMDTypeInt4.h"
#include "naucrates/md/CMDTypeInt4GPDB.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/operators/CLogicalGet.h"
#include "gpopt/operators/CLogicalSelect.h"
#include "gpopt/operators/CScalarConst.h"
#include "gpopt/operators/CScalarOp.h"
#include "gpopt/operators/CScalarIdent.h"
#include "gpopt/metadata/CColumnDescriptor.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/minidump/CMetadataAccessorFactory.h"
#include "gpopt/minidump/CMinidumperUtils.h"
#include "gpopt/translate/CTranslatorDXLToExpr.h"

#define GPDB_INT4_GT_OP OID(521)
#define GPDB_INT4_ADD_OP OID(551)

const CHAR *szQueryDroppedColumn =
	"../data/dxl/expressiontests/NullableDroppedColumn.xml";
const CHAR *szQueryTableScan = "../data/dxl/expressiontests/TableScan.xml";
const CHAR *szQueryLimit = "../data/dxl/expressiontests/LimitQuery.xml";
const CHAR *szQueryLimitNoOffset =
	"../data/dxl/expressiontests/LimitQueryNoOffset.xml";
const CHAR *szQueryTVF =
	"../data/dxl/expressiontests/TableValuedFunctionQuery.xml";
static const CHAR *szQueryScalarSubquery =
	"../data/dxl/expressiontests/ScalarSubqueryQuery.xml";
const CHAR *szScalarConstArray =
	"../data/dxl/expressiontests/CScalarConstArray.xml";

static const CHAR *m_rgszDXLFileNames[] = {
	"../data/dxl/query/dxl-q17.xml",
	"../data/dxl/query/dxl-q18.xml",
	"../data/dxl/query/dxl-q19.xml",
	"../data/dxl/query/dxl-q23.xml",
};
//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprTest::EresUnittest
//
//	@doc:
//		Unittest for translating PlannedStmt to DXL
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTranslatorDXLToExprTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(
			CTranslatorDXLToExprTest::EresUnittest_MetadataColumnMapping),
		GPOS_UNITTEST_FUNC(
			CTranslatorDXLToExprTest::EresUnittest_SingleTableQuery),
		GPOS_UNITTEST_FUNC(CTranslatorDXLToExprTest::EresUnittest_SelectQuery),
		GPOS_UNITTEST_FUNC(
			CTranslatorDXLToExprTest::EresUnittest_SelectQueryWithConst),
		GPOS_UNITTEST_FUNC(
			CTranslatorDXLToExprTest::EresUnittest_SelectQueryWithBoolExpr),
		GPOS_UNITTEST_FUNC(
			CTranslatorDXLToExprTest::EresUnittest_SelectQueryWithScalarOp),
		GPOS_UNITTEST_FUNC(CTranslatorDXLToExprTest::EresUnittest_Limit),
		GPOS_UNITTEST_FUNC(
			CTranslatorDXLToExprTest::EresUnittest_LimitNoOffset),
		GPOS_UNITTEST_FUNC(
			CTranslatorDXLToExprTest::EresUnittest_ScalarSubquery),
		GPOS_UNITTEST_FUNC(CTranslatorDXLToExprTest::EresUnittest_TVF),
		GPOS_UNITTEST_FUNC(
			CTranslatorDXLToExprTest::EresUnittest_SelectQueryWithConstInList)};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprTest::Pexpr
//
//	@doc:
//		The function takes as input to the DXL file containing the plan, and a DXL file
//		containing the meta data, and returns its corresponding Expr Tree.
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExprTest::Pexpr(CMemoryPool *mp, const CHAR *dxl_filename)
{
	// get the dxl document
	CHAR *szDXLExpected = CDXLUtils::Read(mp, dxl_filename);

	// parse the DXL tree from the given DXL document
	CQueryToDXLResult *ptroutput = CDXLUtils::ParseQueryToQueryDXLTree(
		mp, szDXLExpected, NULL /*xsd_file_path*/);

	GPOS_ASSERT(NULL != ptroutput->CreateDXLNode() &&
				NULL != ptroutput->GetOutputColumnsDXLArray());

	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	// translate DXL Tree -> Expr Tree
	CTranslatorDXLToExpr *pdxltr =
		GPOS_NEW(mp) CTranslatorDXLToExpr(mp, md_accessor);

	CExpression *pexprTranslated = pdxltr->PexprTranslateQuery(
		ptroutput->CreateDXLNode(), ptroutput->GetOutputColumnsDXLArray(),
		ptroutput->GetCTEProducerDXLArray());

	//clean up
	GPOS_DELETE(ptroutput);
	GPOS_DELETE(pdxltr);
	GPOS_DELETE_ARRAY(szDXLExpected);
	return pexprTranslated;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprTest::EresTranslateAndCheck
//
//	@doc:
//		Main tool for testing translation of DXL trees into Expr. The function
//		takes as input to a DXL file containing the algebrized query, and a DXL file
//		containing the meta data. Next, translate the DXL into Expr Tree. Lastly,
//		compare the translated Expr Tree against the expected Expr Tree.
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTranslatorDXLToExprTest::EresTranslateAndCheck(
	CMemoryPool *mp, const CHAR *dxl_filename,
	const CWStringDynamic *pstrExpected)
{
	GPOS_RESULT eres = GPOS_FAILED;

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
					 CTestUtils::GetCostModel(mp));

	// translate the DXL document into Expr Tree
	CExpression *pexprTranslated = Pexpr(mp, dxl_filename);

	// get the string representation of the Expr Tree
	CWStringDynamic *pstrTranslated = Pstr(mp, pexprTranslated);

	GPOS_ASSERT(NULL != pstrExpected && NULL != pstrTranslated);

	CWStringDynamic str(mp);
	COstreamString oss(&str);

	// compare the two Expr trees
	if (!pstrExpected->Equals(pstrTranslated))
	{
		oss << "Output does not match expected DXL document" << std::endl;
		oss << "Expected: " << std::endl;
		oss << pstrExpected->GetBuffer() << std::endl;
		oss << "Actual: " << std::endl;
		oss << pstrTranslated->GetBuffer() << std::endl;
	}
	else
	{
		oss << "Output matches expected DXL document" << std::endl;
		eres = GPOS_OK;
	}

	GPOS_TRACE(str.GetBuffer());

	// clean up
	pexprTranslated->Release();
	GPOS_DELETE(pstrExpected);
	GPOS_DELETE(pstrTranslated);

	return eres;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprTest::GetMDName
//
//	@doc:
//		Generate a string representation of an Expr Tree
//
//---------------------------------------------------------------------------
CWStringDynamic *
CTranslatorDXLToExprTest::Pstr(CMemoryPool *mp, CExpression *pexpr)
{
	GPOS_ASSERT(NULL != pexpr);

	CWStringDynamic str(mp);
	COstreamString *poss = GPOS_NEW(mp) COstreamString(&str);

	*poss << std::endl;
	pexpr->OsPrint(*poss);

	CWStringDynamic *pstrExpr =
		GPOS_NEW(mp) CWStringDynamic(mp, str.GetBuffer());

	GPOS_DELETE(poss);

	return pstrExpr;
}


namespace
{
class GetBuilder
{
	CMemoryPool *m_mp;
	CTableDescriptor *m_ptabdesc;
	CWStringConst m_strTableName;
	const IMDTypeInt4 *m_pmdtypeint4;

public:
	GetBuilder(CMemoryPool *mp, CWStringConst strTableName, OID oidTableOid)
		: m_mp(mp), m_strTableName(strTableName)
	{
		CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
		m_pmdtypeint4 =
			md_accessor->PtMDType<IMDTypeInt4>(CTestUtils::m_sysidDefault);
		CMDIdGPDB *mdid = GPOS_NEW(mp) CMDIdGPDB(oidTableOid, 1, 1);

		const BOOL convert_hash_to_random = false;
		const ULONG ulExecuteAsUser = 0;
		m_ptabdesc = GPOS_NEW(mp) CTableDescriptor(
			mp, mdid, CName(&strTableName), convert_hash_to_random,
			CMDRelationGPDB::EreldistrMasterOnly,
			CMDRelationGPDB::ErelstorageHeap, ulExecuteAsUser);
	}

	void
	AddIntColumn(CWStringConst strColumnName, int attno, BOOL is_nullable)
	{
		CColumnDescriptor *pcoldesc = GPOS_NEW(m_mp)
			CColumnDescriptor(m_mp, m_pmdtypeint4, default_type_modifier,
							  CName(&strColumnName), attno, is_nullable);
		m_ptabdesc->AddColumn(pcoldesc);
	}

	CExpression *
	PexprLogicalGet()
	{
		return CTestUtils::PexprLogicalGet(m_mp, m_ptabdesc, &m_strTableName);
	}
};
}  // namespace


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprTest::PexprGet
//
//	@doc:
//		Create a get expression for a table (r) with two integer columns a and b
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExprTest::PexprGet(CMemoryPool *mp)
{
	CWStringConst strTblName(GPOS_WSZ_LIT("r"));
	const BOOL is_nullable = true;

	GetBuilder gb(mp, strTblName, GPOPT_TEST_REL_OID21);
	gb.AddIntColumn(CWStringConst(GPOS_WSZ_LIT("a")), 1, is_nullable);
	gb.AddIntColumn(CWStringConst(GPOS_WSZ_LIT("b")), 2, is_nullable);

	return gb.PexprLogicalGet();
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprTest::EresUnittest_SingleTableQuery
//
//	@doc:
//		Test translating a DXL Tree for the query (select * from r)
//		into Expr Tree
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTranslatorDXLToExprTest::EresUnittest_SingleTableQuery()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CWStringDynamic *pstrExpected = NULL;

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// manually create the Expr Tree
	{
		// install opt context in TLS
		CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
						 CTestUtils::GetCostModel(mp));
		CExpression *pexprExpected = PexprGet(mp);
		pstrExpected = Pstr(mp, pexprExpected);

		//clean up
		pexprExpected->Release();
	}

	return EresTranslateAndCheck(mp, szQueryTableScan, pstrExpected);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprTest::EresUnittest_TVF
//
//	@doc:
//		Test translating a DXL Tree for the query
//		(select * from generate_series(1, 10)) into Expr Tree
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTranslatorDXLToExprTest::EresUnittest_TVF()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CWStringDynamic *pstrExpected = NULL;

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// manually create the Expr Tree
	{
		// install opt context in TLS
		CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
						 CTestUtils::GetCostModel(mp));
		CExpression *pexprExpected = CTestUtils::PexprLogicalTVFTwoArgs(mp);
		pstrExpected = Pstr(mp, pexprExpected);

		//clean up
		pexprExpected->Release();
	}

	return EresTranslateAndCheck(mp, szQueryTVF, pstrExpected);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprTest::EresUnittest_SelectQuery
//
//	@doc:
//		Test translating a DXL Tree for the query
//		(select * from r where a = b) into Expr Tree
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTranslatorDXLToExprTest::EresUnittest_SelectQuery()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CWStringDynamic *pstrExpected = NULL;

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// manually create the Expr Tree
	{
		// install opt context in TLS
		CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
						 CTestUtils::GetCostModel(mp));

		CExpression *pexprLgGet = PexprGet(mp);

		CLogicalGet *popGet = CLogicalGet::PopConvert(pexprLgGet->Pop());


		// the output column references from the logical get
		CColRefArray *colref_array = popGet->PdrgpcrOutput();

		GPOS_ASSERT(NULL != colref_array && 2 == colref_array->Size());

		CColRef *pcrLeft = (*colref_array)[0];
		CColRef *pcrRight = (*colref_array)[1];

		CExpression *pexprPredicate =
			CUtils::PexprScalarEqCmp(mp, pcrLeft, pcrRight);

		CExpression *pexprExpected =
			CUtils::PexprLogicalSelect(mp, pexprLgGet, pexprPredicate);
		pstrExpected = Pstr(mp, pexprExpected);

		//clean up
		pexprExpected->Release();
	}

	return EresTranslateAndCheck(mp, m_rgszDXLFileNames[0], pstrExpected);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprTest::EresUnittest_SelectQueryWithConst
//
//	@doc:
//		Test translating a DXL Tree for the query (select * from r where a = 5)
//		into Expr Tree
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTranslatorDXLToExprTest::EresUnittest_SelectQueryWithConst()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CWStringDynamic *pstrExpected = NULL;

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// manually create the Expr Tree
	{
		// install opt context in TLS
		CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
						 CTestUtils::GetCostModel(mp));
		CExpression *pexprLgGet = PexprGet(mp);
		CLogicalGet *popGet = CLogicalGet::PopConvert(pexprLgGet->Pop());

		// the output column references from the logical get
		CColRefArray *colref_array = popGet->PdrgpcrOutput();
		GPOS_ASSERT(NULL != colref_array && 2 == colref_array->Size());

		CColRef *pcrLeft = (*colref_array)[0];
		ULONG ulVal = 5;
		CExpression *pexprScConst = CUtils::PexprScalarConstInt4(mp, ulVal);
		CExpression *pexprScCmp =
			CUtils::PexprScalarEqCmp(mp, pcrLeft, pexprScConst);

		CExpression *pexprExpected =
			CUtils::PexprLogicalSelect(mp, pexprLgGet, pexprScCmp);
		pstrExpected = Pstr(mp, pexprExpected);

		//clean up
		pexprExpected->Release();
	}

	return EresTranslateAndCheck(mp, m_rgszDXLFileNames[1], pstrExpected);
}


// Test translating a DXL Tree for the query (select * from r where a in (5,6,7))
// into Expr Tree
GPOS_RESULT
CTranslatorDXLToExprTest::EresUnittest_SelectQueryWithConstInList()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CWStringDynamic *pstrExpected = NULL;

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// manually create the Expr Tree
	{
		// install opt context in TLS
		CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
						 CTestUtils::GetCostModel(mp));
		CExpression *pexprLgGet = PexprGet(mp);
		CLogicalGet *popGet = CLogicalGet::PopConvert(pexprLgGet->Pop());

		// the output column references from the logical get
		CColRefArray *colref_array = popGet->PdrgpcrOutput();
		GPOS_ASSERT(NULL != colref_array && 2 == colref_array->Size());

		CColRef *colref = (*colref_array)[0];
		ULONG ulVal1 = 5;
		CExpression *pexprScConst1 = CUtils::PexprScalarConstInt4(mp, ulVal1);
		ULONG ulVal2 = 6;
		CExpression *pexprScConst2 = CUtils::PexprScalarConstInt4(mp, ulVal2);
		ULONG ulVal3 = 7;
		CExpression *pexprScConst3 = CUtils::PexprScalarConstInt4(mp, ulVal3);
		CExpressionArray *pexprScalarChildren =
			GPOS_NEW(mp) CExpressionArray(mp);
		pexprScalarChildren->Append(pexprScConst1);
		pexprScalarChildren->Append(pexprScConst2);
		pexprScalarChildren->Append(pexprScConst3);

		CExpression *pexprScalarArrayCmp = CUtils::PexprScalarArrayCmp(
			mp, CScalarArrayCmp::EarrcmpAny, IMDType::EcmptEq,
			pexprScalarChildren, colref);
		CExpression *pexprScalarArrayCmpCollapsed =
			CUtils::PexprCollapseConstArray(mp, pexprScalarArrayCmp);
		pexprScalarArrayCmp->Release();
		CExpression *pexprExpected = CUtils::PexprLogicalSelect(
			mp, pexprLgGet, pexprScalarArrayCmpCollapsed);
		pstrExpected = Pstr(mp, pexprExpected);

		//clean up
		pexprExpected->Release();
	}

	return EresTranslateAndCheck(mp, szScalarConstArray, pstrExpected);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprTest::EresUnittest_SelectQueryWithBoolExpr
//
//	@doc:
//		Test translating a DXL Tree for the query
//		(select * from r where a = 5 and a = b) into Expr Tree
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTranslatorDXLToExprTest::EresUnittest_SelectQueryWithBoolExpr()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CWStringDynamic *pstrExpected = NULL;

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// manually create the Expr Tree
	{
		// install opt context in TLS
		CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
						 CTestUtils::GetCostModel(mp));

		CExpression *pexprLgGet = PexprGet(mp);

		CLogicalGet *popGet = CLogicalGet::PopConvert(pexprLgGet->Pop());

		// the output column references from the logical get
		CColRefArray *colref_array = popGet->PdrgpcrOutput();

		GPOS_ASSERT(NULL != colref_array && 2 == colref_array->Size());

		// create a scalar compare for a = 5
		CColRef *pcrLeft = (*colref_array)[0];

		ULONG ulVal = 5;
		CExpression *pexprScConst = CUtils::PexprScalarConstInt4(mp, ulVal);

		CExpression *pexprScCmp =
			CUtils::PexprScalarEqCmp(mp, pcrLeft, pexprScConst);

		// create a scalar compare for a = b
		CColRef *pcrRight = (*colref_array)[1];
		CExpressionArray *pdrgpexprInput = GPOS_NEW(mp) CExpressionArray(mp, 2);
		pdrgpexprInput->Append(pexprScCmp);
		pexprScCmp = CUtils::PexprScalarEqCmp(mp, pcrLeft, pcrRight);

		pdrgpexprInput->Append(pexprScCmp);
		CExpression *pexprScBool = CUtils::PexprScalarBoolOp(
			mp, CScalarBoolOp::EboolopAnd, pdrgpexprInput);
		CExpression *pexprExpected =
			CUtils::PexprLogicalSelect(mp, pexprLgGet, pexprScBool);
		pstrExpected = Pstr(mp, pexprExpected);

		//clean up
		pexprExpected->Release();
	}

	return EresTranslateAndCheck(mp, m_rgszDXLFileNames[2], pstrExpected);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprTest::EresUnittest_SelectQueryWithScalarOp
//
//	@doc:
//		Test translating a DXL Tree for the query
//		(select * from r where a > b + 2) into Expr Tree
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTranslatorDXLToExprTest::EresUnittest_SelectQueryWithScalarOp()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CWStringDynamic *pstrExpected = NULL;

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// manually create the Expr Tree
	{
		// install opt context in TLS
		CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
						 CTestUtils::GetCostModel(mp));

		CExpression *pexprLgGet = PexprGet(mp);

		CLogicalGet *popGet = CLogicalGet::PopConvert(pexprLgGet->Pop());

		// the output column references from the logical get
		CColRefArray *colref_array = popGet->PdrgpcrOutput();

		GPOS_ASSERT(NULL != colref_array && 2 == colref_array->Size());

		// create a scalar op (arithmetic) for b + 2
		CColRef *pcrLeft = (*colref_array)[1];

		ULONG ulVal = 2;
		CExpression *pexprScConst = CUtils::PexprScalarConstInt4(mp, ulVal);
		CExpression *pexprScOp = CUtils::PexprScalarOp(
			mp, pcrLeft, pexprScConst, CWStringConst(GPOS_WSZ_LIT("+")),
			GPOS_NEW(mp) CMDIdGPDB(GPDB_INT4_ADD_OP));

		// create a scalar compare for a > b + 2
		CColRef *pcrRight = (*colref_array)[0];
		CExpression *pexprScCmp = CUtils::PexprScalarCmp(
			mp, pcrRight, pexprScOp, CWStringConst(GPOS_WSZ_LIT(">")),
			GPOS_NEW(mp) CMDIdGPDB(GPDB_INT4_GT_OP));

		CExpression *pexprExpected =
			CUtils::PexprLogicalSelect(mp, pexprLgGet, pexprScCmp);
		pstrExpected = Pstr(mp, pexprExpected);

		//clean up
		pexprExpected->Release();
	}

	return EresTranslateAndCheck(mp, m_rgszDXLFileNames[3], pstrExpected);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprTest::EresUnittest_Limit
//
//	@doc:
//		Test translating a DXL Tree for an order by-limit query
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTranslatorDXLToExprTest::EresUnittest_Limit()
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

	// translate the DXL document into Expr Tree
	CExpression *pexprTranslated = Pexpr(mp, szQueryLimit);

	CWStringDynamic str(mp);
	COstreamString *poss = GPOS_NEW(mp) COstreamString(&str);

	*poss << std::endl;

	pexprTranslated->OsPrint(*poss);

	GPOS_TRACE(str.GetBuffer());

	pexprTranslated->Release();
	GPOS_DELETE(poss);

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprTest::EresUnittest_LimitNoOffset
//
//	@doc:
//		Test translating a DXL Tree for an order by-limit query with no offset specified
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTranslatorDXLToExprTest::EresUnittest_LimitNoOffset()
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

	// translate the DXL document into Expr Tree
	CExpression *pexprTranslated = Pexpr(mp, szQueryLimitNoOffset);

	CWStringDynamic str(mp);
	COstreamString *poss = GPOS_NEW(mp) COstreamString(&str);

	*poss << std::endl;

	pexprTranslated->OsPrint(*poss);

	GPOS_TRACE(str.GetBuffer());

	pexprTranslated->Release();
	GPOS_DELETE(poss);
	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExprTest::EresUnittest_ScalarSubquery
//
//	@doc:
//		Test translating a DXL query with a scalar subquery
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTranslatorDXLToExprTest::EresUnittest_ScalarSubquery()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(mp, CMDCache::Pcache());
	mda.RegisterProvider(CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(mp, &mda, NULL, /* pceeval */
					 CTestUtils::GetCostModel(mp));

	// translate the DXL document into Expr Tree
	CExpression *pexprTranslated = Pexpr(mp, szQueryScalarSubquery);

	CWStringDynamic str(mp);
	COstreamString *poss = GPOS_NEW(mp) COstreamString(&str);

	*poss << std::endl;

	pexprTranslated->OsPrint(*poss);

	GPOS_TRACE(str.GetBuffer());

	pexprTranslated->Release();
	GPOS_DELETE(poss);
	return GPOS_OK;
}

GPOS_RESULT
CTranslatorDXLToExprTest::EresUnittest_MetadataColumnMapping()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CAutoP<CDXLMinidump> apdxlmd(
		CMinidumperUtils::PdxlmdLoad(mp, szQueryDroppedColumn));

	CMetadataAccessorFactory factory(mp, apdxlmd.Value(), szQueryDroppedColumn);

	CAutoOptCtxt aoc(mp, factory.Pmda(), NULL, CTestUtils::GetCostModel(mp));

	CAutoRef<CExpression> apExpr(
		CTranslatorDXLToExprTest::Pexpr(mp, szQueryDroppedColumn));

	CLogicalGet *pActualGet = (CLogicalGet *) apExpr->Pop();
	CColumnDescriptorArray *pDrgColDesc =
		pActualGet->Ptabdesc()->Pdrgpcoldesc();
	CColumnDescriptor *pColDesc = (*pDrgColDesc)[0];
	bool actualNullable = pColDesc->IsNullable();
	GPOS_RTL_ASSERT(actualNullable == false);

	return GPOS_OK;
}

// EOF

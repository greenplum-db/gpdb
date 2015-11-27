//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CTranslatorPlStmtToDXL.cpp
//
//	@doc:
//		Implementation of the methods for translating GPDB's PlannedStmt into DXL Tree.
//		All translator methods allocate memory in the provided memory pool, and
//		the caller is responsible for freeing it
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "postgres.h"

#include "nodes/plannodes.h"
#include "nodes/parsenodes.h"
#include "access/sysattr.h"
#include "optimizer/walkers.h"

#include "gpos/base.h"
#include "gpos/string/CWStringDynamic.h"

#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/translate/CTranslatorPlStmtToDXL.h"
#include "gpopt/translate/CTranslatorUtils.h"
#include "gpopt/translate/CMappingVarColId.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/dxltokens.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/gpdb_types.h"

#include "naucrates/md/IMDScalarOp.h"
#include "naucrates/md/IMDRelation.h"
#include "naucrates/md/IMDIndex.h"

#include "gpopt/gpdbwrappers.h"

using namespace gpdxl;
using namespace gpos;
using namespace gpopt;

#define DEFAULT_QUERY_LEVEL 0

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::CTranslatorPlStmtToDXL
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CTranslatorPlStmtToDXL::CTranslatorPlStmtToDXL
	(
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	CIdGenerator *pulIdGenerator,
	PlannedStmt *pplstmt,
	CMappingParamIdScalarId *pmapps
	)
	:
	m_pmp(pmp),
	m_pmda(pmda),
	m_pidgtor(pulIdGenerator),
	m_pplstmt(pplstmt),
	m_pparammapping(pmapps)
{
	GPOS_ASSERT(NULL != m_pplstmt);
	m_psctranslator = New(m_pmp)
							CTranslatorScalarToDXL
								(
								m_pmp,
								m_pmda,
								pulIdGenerator,
								NULL, // cte id generator
								0, // m_ulQueryLevel
								false, // m_fQuery
								m_pplstmt,
								m_pparammapping,
								NULL,  // CTE Mapping
								NULL // pdrgpdxlnCTE
								);
	m_phmuldxlnSharedScanProjLists = New(m_pmp) HMUlPdxln(m_pmp);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::~CTranslatorPlStmtToDXL
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CTranslatorPlStmtToDXL::~CTranslatorPlStmtToDXL()
{
	m_phmuldxlnSharedScanProjLists->Release();
	delete m_psctranslator;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnFromPlstmt
//
//	@doc:
//		Translates a PlannedStmt into a DXL tree.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnFromPlstmt
	()
{
	if (CMD_SELECT != m_pplstmt->commandType)
	{
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiPlStmt2DXLConversion,
			GPOS_WSZ_LIT("DML operations")
			);
	}

	if (NULL != m_pplstmt->intoClause)
	{
		GPOS_RAISE
			(
			gpdxl::ExmaDXL,
			gpdxl::ExmiPlStmt2DXLConversion,
			GPOS_WSZ_LIT("Select into")
			);
	}

	return PdxlnFromPlan(m_pplstmt->planTree);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnFromPlan
//
//	@doc:
//		Translates a Plan operator tree into a DXL tree.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnFromPlan
	(
	const Plan *pplan
	)
{
	GPOS_ASSERT(NULL != pplan);

	STranslatorElem rgTranslators[] =
	{
		{T_Scan, &CTranslatorPlStmtToDXL::PdxlnTblScanFromPlan},
		{T_IndexScan, &CTranslatorPlStmtToDXL::PdxlnIndexScanFromPlan},
	//	{T_IndexOnlyScan, &CTranslatorPlStmtToDXL::PdxlnIndexOnlyScanFromPlan},
		{T_SeqScan, &CTranslatorPlStmtToDXL::PdxlnTblScanFromPlan},
		{T_HashJoin, &CTranslatorPlStmtToDXL::PdxlnHashjoinFromPlan},
		{T_Hash, &CTranslatorPlStmtToDXL::PdxlnHashFromPlan},
		{T_NestLoop, &CTranslatorPlStmtToDXL::PdxlnNLJoinFromPlan},
		{T_MergeJoin, &CTranslatorPlStmtToDXL::PdxlnMergeJoinFromPlan},
		{T_Motion, &CTranslatorPlStmtToDXL::PdxlnMotionFromPlan},
		{T_Limit, &CTranslatorPlStmtToDXL::PdxlnLimitFromPlan},
		{T_Agg, &CTranslatorPlStmtToDXL::PdxlnAggFromPlan},
		{T_Window, &CTranslatorPlStmtToDXL::PdxlnWindowFromPlan},
		{T_Unique, &CTranslatorPlStmtToDXL::PdxlnUniqueFromPlan},
		{T_Sort, &CTranslatorPlStmtToDXL::PdxlnSortFromPlan},
		{T_SubqueryScan, &CTranslatorPlStmtToDXL::PdxlnSubqueryScanFromPlan},
		{T_Append, &CTranslatorPlStmtToDXL::PdxlnAppendFromPlan},
		{T_Result, &CTranslatorPlStmtToDXL::PdxlnResultFromPlan},
		{T_Material, &CTranslatorPlStmtToDXL::PdxlnMaterializeFromPlan},
		{T_ShareInputScan, &CTranslatorPlStmtToDXL::PdxlnSharedScanFromPlan},
		{T_Sequence, &CTranslatorPlStmtToDXL::PdxlnSequence},
		{T_DynamicTableScan, &CTranslatorPlStmtToDXL::PdxlnDynamicTableScan},
		{T_FunctionScan, &CTranslatorPlStmtToDXL::PdxlnFunctionScanFromPlan},
	};

	const ULONG ulTranslators = GPOS_ARRAY_SIZE(rgTranslators);

	NodeTag ent = pplan->type;

	// find translator for the expression type
	PfPdxln pf = NULL;
	for (ULONG ul = 0; ul < ulTranslators; ul++)
	{
		STranslatorElem elem = rgTranslators[ul];
		if (ent == elem.ent)
		{
			pf = elem.pf;
			break;
		}
	}

	if (NULL == pf)
	{
		CHAR *sz = (CHAR*) gpdb::SzNodeToString(const_cast<Plan*>(pplan));
		CWStringDynamic *pstr = CDXLUtils::PstrFromSz(m_pmp, sz);

		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiPlStmt2DXLConversion, pstr->Wsz());
	}

	return (this->*pf)(pplan);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnHashFromPlan
//
//	@doc:
//		Create a DXL hash node from a GPDB hash plan node.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnHashFromPlan
	(
	const Plan *pplan
	)
{
	return PdxlnFromPlan(pplan->lefttree);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnHashjoinFromPlan
//
//	@doc:
//		Create a DXL hash join node from a GPDB hash join plan node.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnHashjoinFromPlan
	(
	const Plan *pplan
	)
{
	GPOS_ASSERT(NULL != pplan);
	GPOS_ASSERT(IsA(pplan, HashJoin));
	const HashJoin *phj = (HashJoin *) pplan;

	// extract plan properties
	CDXLPhysicalProperties *pdxlprop = PdxlpropFromPlan(pplan);

	GPOS_ASSERT(NULL != pdxlprop);

	EdxlJoinType edxljt = CTranslatorUtils::EdxljtFromJoinType((phj->join).jointype);

	// construct hash join operator
	CDXLPhysicalHashJoin *pdxlopHj = New(m_pmp) CDXLPhysicalHashJoin(m_pmp, edxljt);

	// construct hash join operator node
	CDXLNode *pdxlnHJ = New(m_pmp) CDXLNode(m_pmp, pdxlopHj);
	pdxlnHJ->SetProperties(pdxlprop);

	// translate left and right child
	Plan *pplanLeft = outerPlan(phj);
	Plan *pplanRight = innerPlan(phj);

	GPOS_ASSERT(NULL != pplanLeft);
	GPOS_ASSERT(NULL != pplanRight);

	CDXLNode *pdxlnLeft = PdxlnFromPlan(pplanLeft);
	CDXLNode *pdxlnRight = PdxlnFromPlan(pplanRight);

	GPOS_ASSERT(1 <= pdxlnLeft->UlArity());
	GPOS_ASSERT(1 <= pdxlnRight->UlArity());

	// get left and right projection lists
	CDXLNode *pdxlnPrLLeft = (*pdxlnLeft)[0];
	CDXLNode *pdxlnPrLRight = (*pdxlnRight)[0];

	GPOS_ASSERT(pdxlnPrLLeft->Pdxlop()->Edxlop() == EdxlopScalarProjectList);
	GPOS_ASSERT(pdxlnPrLRight->Pdxlop()->Edxlop() == EdxlopScalarProjectList);

	// translate hash condition
	CDXLNode *pdxlnHashCondList = New(m_pmp) CDXLNode(m_pmp, New(m_pmp) CDXLScalarHashCondList(m_pmp));

	List *plHashClauses = NIL;

	m_psctranslator->SetCallingPhysicalOpType(EpspotHashjoin);

	// if all hash conditions are equality tests, hashclauses contain them;
	// if there are is not distinct from (INDF) tests, the conditions are in hashqualclauses
	if (NIL != phj->hashqualclauses)
	{
		// there are INDF conditions
		plHashClauses = phj->hashqualclauses;
	}
	else
	{
		// all clauses are equality checks
		plHashClauses = phj->hashclauses;
	}

	const ULONG ulLen = gpdb::UlListLength(plHashClauses);
	for (ULONG ul = 0; ul < ulLen; ul++)
	{
		Expr *pexpr = (Expr *) gpdb::PvListNth(plHashClauses, ul);

		CMappingVarColId mapvarcolid(m_pmp);

		mapvarcolid.LoadProjectElements(DEFAULT_QUERY_LEVEL, OUTER, pdxlnPrLLeft);

		mapvarcolid.LoadProjectElements(DEFAULT_QUERY_LEVEL, INNER, pdxlnPrLRight);

		CDXLNode *pdxlnHashCond = m_psctranslator->PdxlnScOpFromExpr
									(
									pexpr,
									&mapvarcolid
									);

		pdxlnHashCondList->AddChild(pdxlnHashCond);
	}

	// translate target list and additional join condition
	CDXLNode *pdxlnPrL = NULL;
	CDXLNode *pdxlnFilter = NULL;

	TranslateTListAndQual(
		(phj->join).plan.targetlist,
		(phj->join).plan.qual,
		0,				//ulRTEIndex
		NULL, 			//pdxltabdesc
		NULL,			//pmdindex
		pdxlnPrLLeft,
		pdxlnPrLRight,
		&pdxlnPrL,
		&pdxlnFilter
		);


	GPOS_ASSERT(NULL != pdxlnPrL);

	CMappingVarColId mapvarcolid(m_pmp);
	mapvarcolid.LoadProjectElements(DEFAULT_QUERY_LEVEL, OUTER, pdxlnPrLLeft);

	mapvarcolid.LoadProjectElements(DEFAULT_QUERY_LEVEL, INNER, pdxlnPrLRight);

	CDXLNode *pdxlnJoinFilter = m_psctranslator->PdxlnFilterFromQual
								(
								(phj->join).joinqual,
								&mapvarcolid,
								EdxlopScalarJoinFilter
								);

	// add children in the right order
	pdxlnHJ->AddChild(pdxlnPrL);		// proj list
	pdxlnHJ->AddChild(pdxlnFilter);		// filter
	pdxlnHJ->AddChild(pdxlnJoinFilter);	// additional join filter
	pdxlnHJ->AddChild(pdxlnHashCondList);	// hash join condition
	pdxlnHJ->AddChild(pdxlnLeft);		// left child
	pdxlnHJ->AddChild(pdxlnRight);		// right child

	return pdxlnHJ;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnNLJoinFromPlan
//
//	@doc:
//		Create a DXL nested loop join node from a GPDB NestLoop plan node.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnNLJoinFromPlan
	(
	const Plan *pplan
	)
{
	GPOS_ASSERT(NULL != pplan);
	GPOS_ASSERT(IsA(pplan, NestLoop));
	const NestLoop *pnlj = (NestLoop *) pplan;

	// extract plan properties
	CDXLPhysicalProperties *pdxlprop = PdxlpropFromPlan(pplan);

	GPOS_ASSERT(NULL != pdxlprop);

	EdxlJoinType edxljt = CTranslatorUtils::EdxljtFromJoinType((pnlj->join).jointype);

	BOOL fIndexNLJ = IsA(pplan->righttree, IndexScan);

	// construct nested loop join operator
	CDXLPhysicalNLJoin *pdxlnlj = New(m_pmp) CDXLPhysicalNLJoin(m_pmp, edxljt, fIndexNLJ);

	// construct nested loop join operator node
	CDXLNode *pdxlnNLJ = New(m_pmp) CDXLNode(m_pmp, pdxlnlj);
	pdxlnNLJ->SetProperties(pdxlprop);

	// translate left and right child
	Plan *pplanLeft = outerPlan(pnlj);
	Plan *pplanRight = innerPlan(pnlj);

	GPOS_ASSERT(NULL != pplanLeft);
	GPOS_ASSERT(NULL != pplanRight);

	CDXLNode *pdxlnLeft = PdxlnFromPlan(pplanLeft);
	CDXLNode *pdxlnRight = PdxlnFromPlan(pplanRight);

	GPOS_ASSERT(1 <= pdxlnLeft->UlArity());
	GPOS_ASSERT(1 <= pdxlnRight->UlArity());

	// get left and right projection lists
	CDXLNode *pdxlnPrLLeft = (*pdxlnLeft)[0];
	CDXLNode *pdxlnPrLRight = (*pdxlnRight)[0];

	GPOS_ASSERT(pdxlnPrLLeft->Pdxlop()->Edxlop() == EdxlopScalarProjectList);
	GPOS_ASSERT(pdxlnPrLRight->Pdxlop()->Edxlop() == EdxlopScalarProjectList);

	// translate target list and join filter
	CDXLNode *pdxlnPrL = NULL;
	CDXLNode *pdxlnFilter = NULL;

	m_psctranslator->SetCallingPhysicalOpType(EpspotNLJoin);

	TranslateTListAndQual(
		(pnlj->join).plan.targetlist,
		(pnlj->join).plan.qual,
		0,				//ulRTEIndex
		NULL, 			//pdxltabdesc
		NULL,			//pmdindex
		pdxlnPrLLeft,
		pdxlnPrLRight,
		&pdxlnPrL,
		&pdxlnFilter
		);

	CMappingVarColId mapvarcolid(m_pmp);
	mapvarcolid.LoadProjectElements(DEFAULT_QUERY_LEVEL, OUTER, pdxlnPrLLeft);
	mapvarcolid.LoadProjectElements(DEFAULT_QUERY_LEVEL, INNER, pdxlnPrLRight);

	CDXLNode *pdxlnJoinFilter = m_psctranslator->PdxlnFilterFromQual((pnlj->join).joinqual, &mapvarcolid, EdxlopScalarJoinFilter);

	// add children in the right order
	pdxlnNLJ->AddChild(pdxlnPrL);			// proj list
	pdxlnNLJ->AddChild(pdxlnFilter);		// plan filter
	pdxlnNLJ->AddChild(pdxlnJoinFilter);	// additional join filter
	pdxlnNLJ->AddChild(pdxlnLeft);			// left child
	pdxlnNLJ->AddChild(pdxlnRight);			// right child

	return pdxlnNLJ;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnMergeJoinFromPlan
//
//	@doc:
//		Create a DXL merge join node from a GPDB merge join plan node.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnMergeJoinFromPlan
	(
	const Plan *pplan
	)
{
	GPOS_ASSERT(NULL != pplan);
	GPOS_ASSERT(IsA(pplan, MergeJoin));
	const MergeJoin *pmj = (MergeJoin *) pplan;

	// extract plan properties
	CDXLPhysicalProperties *pdxlprop = PdxlpropFromPlan(pplan);

	GPOS_ASSERT(NULL != pdxlprop);

	EdxlJoinType edxljt = CTranslatorUtils::EdxljtFromJoinType((pmj->join).jointype);

	// construct hash join operator
	CDXLPhysicalMergeJoin *pdxlopMj = New(m_pmp) CDXLPhysicalMergeJoin(m_pmp, edxljt, pmj->unique_outer);

	// construct merge join operator node
	CDXLNode *pdxlnMJ = New(m_pmp) CDXLNode(m_pmp, pdxlopMj);
	pdxlnMJ->SetProperties(pdxlprop);

	// translate left and right child
	Plan *pplanLeft = outerPlan(pmj);
	Plan *pplanRight = innerPlan(pmj);

	GPOS_ASSERT(NULL != pplanLeft);
	GPOS_ASSERT(NULL != pplanRight);

	CDXLNode *pdxlnLeft = PdxlnFromPlan(pplanLeft);
	CDXLNode *pdxlnRight = PdxlnFromPlan(pplanRight);

	GPOS_ASSERT(1 <= pdxlnLeft->UlArity());
	GPOS_ASSERT(1 <= pdxlnRight->UlArity());

	// get left and right projection lists
	CDXLNode *pdxlnPrLLeft = (*pdxlnLeft)[0];
	CDXLNode *pdxlnPrLRight = (*pdxlnRight)[0];

	GPOS_ASSERT(pdxlnPrLLeft->Pdxlop()->Edxlop() == EdxlopScalarProjectList);
	GPOS_ASSERT(pdxlnPrLRight->Pdxlop()->Edxlop() == EdxlopScalarProjectList);

	// translate hash condition
	CDXLNode *pdxlnMergeCondList = New(m_pmp) CDXLNode(m_pmp, New(m_pmp) CDXLScalarMergeCondList(m_pmp));

	m_psctranslator->SetCallingPhysicalOpType(EpspotMergeJoin);

	List *plMergeClauses = pmj->mergeclauses;

	const ULONG ulLength = (ULONG) gpdb::UlListLength(plMergeClauses);
	for (ULONG ul = 0; ul < ulLength; ul++)
	{
		Expr *pexpr = (Expr *) gpdb::PvListNth(plMergeClauses, ul);

		CMappingVarColId mapvarcolid(m_pmp);
		mapvarcolid.LoadProjectElements(DEFAULT_QUERY_LEVEL, OUTER, pdxlnPrLLeft);
		mapvarcolid.LoadProjectElements(DEFAULT_QUERY_LEVEL, INNER, pdxlnPrLRight);

		CDXLNode *pdxlnMergeCond = m_psctranslator->PdxlnScOpFromExpr
											(
											pexpr,
											&mapvarcolid
											);

		pdxlnMergeCondList->AddChild(pdxlnMergeCond);
	}

	// translate target list and additional join condition
	CDXLNode *pdxlnPrL = NULL;

	CDXLNode *pdxlnFilter = NULL;

	TranslateTListAndQual(
		(pmj->join).plan.targetlist,
		(pmj->join).plan.qual,
		0,				//ulRTEIndex
		NULL, 			//pdxltabdesc
		NULL,			//pmdindex
		pdxlnPrLLeft,
		pdxlnPrLRight,
		&pdxlnPrL,
		&pdxlnFilter
		);


	GPOS_ASSERT(NULL != pdxlnPrL);

	CMappingVarColId mapvarcolid(m_pmp);
	mapvarcolid.LoadProjectElements(DEFAULT_QUERY_LEVEL, OUTER, pdxlnPrLLeft);
	mapvarcolid.LoadProjectElements(DEFAULT_QUERY_LEVEL, INNER, pdxlnPrLRight);

	CDXLNode *pdxlnJoinFilter = m_psctranslator->PdxlnFilterFromQual
								(
								(pmj->join).joinqual,
								&mapvarcolid,
								EdxlopScalarJoinFilter
								);

	// add children in the right order
	pdxlnMJ->AddChild(pdxlnPrL);			// proj list
	pdxlnMJ->AddChild(pdxlnFilter);			// filter
	pdxlnMJ->AddChild(pdxlnJoinFilter);		// additional join filter
	pdxlnMJ->AddChild(pdxlnMergeCondList);	// merge join condition
	pdxlnMJ->AddChild(pdxlnLeft);			// left child
	pdxlnMJ->AddChild(pdxlnRight);			// right child

	return pdxlnMJ;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnResultFromPlan
//
//	@doc:
//		Create a DXL result node from a GPDB result node.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnResultFromPlan
	(
	const Plan *pplan
	)
{
	GPOS_ASSERT(NULL != pplan);
	GPOS_ASSERT(IsA(pplan, Result));
	const Result *pnresult = (Result *) pplan;

	// extract plan properties
	CDXLPhysicalProperties *pdxlprop = PdxlpropFromPlan(pplan);

	GPOS_ASSERT(NULL != pdxlprop);

	// construct result operator
	CDXLPhysicalResult *pdxlopResult = New(m_pmp) CDXLPhysicalResult(m_pmp);

	GPOS_ASSERT(innerPlan(pnresult) == NULL && "Result node cannot have right child");

	// translate child if there is one
	Plan *pplanChild = outerPlan(pnresult);
	CDXLNode *pdxlnChild = NULL;
	CDXLNode *pdxlnPrLChild = NULL;

	if (NULL != pplanChild)
	{
		pdxlnChild = PdxlnFromPlan(pplanChild);
		GPOS_ASSERT(1 <= pdxlnChild->UlArity());

		// get child's projection lists
		pdxlnPrLChild = (*pdxlnChild)[0];
		GPOS_ASSERT(pdxlnPrLChild->Pdxlop()->Edxlop() == EdxlopScalarProjectList);
	}

	// translate target list and filters
	CDXLNode *pdxlnPrL = NULL;
	CDXLNode *pdxlnFilter = NULL;

	// assert that resconstantqual is actually a list
	GPOS_ASSERT_IMP(NULL != pnresult->resconstantqual, nodeTag(pnresult->resconstantqual) == T_List);

	m_psctranslator->SetCallingPhysicalOpType(EpspotResult);

	TranslateTListAndQual
			(
			(pnresult->plan).targetlist,
			(pnresult->plan).qual,
			0,				//ulRTEIndex
			NULL,
			NULL,			//pmdindex
			pdxlnPrLChild,
			NULL,
			&pdxlnPrL,
			&pdxlnFilter
			);

	CMappingVarColId mapvarcolid(m_pmp);
	mapvarcolid.LoadProjectElements(DEFAULT_QUERY_LEVEL, OUTER, pdxlnPrL);

	CDXLNode *pdxlnOneTimeFilter = m_psctranslator->PdxlnFilterFromQual
								(
								(List *) (pnresult->resconstantqual),
								&mapvarcolid,
								EdxlopScalarOneTimeFilter
								);

	// construct motion operator node
	CDXLNode *pdxln = New(m_pmp) CDXLNode(m_pmp, pdxlopResult);
	pdxln->SetProperties(pdxlprop);

	// add children in the right order
	pdxln->AddChild(pdxlnPrL);				// proj list
	pdxln->AddChild(pdxlnFilter);			// plan filter
	pdxln->AddChild(pdxlnOneTimeFilter);	// one time filter

	if (NULL != pdxlnChild)
	{
		pdxln->AddChild(pdxlnChild);
	}

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnMotionFromPlan
//
//	@doc:
//		Create a DXL motion node from a GPDB Motion plan node.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnMotionFromPlan
	(
	const Plan *pplan
	)
{
	GPOS_ASSERT(NULL != pplan);
 	GPOS_ASSERT(IsA(pplan, Motion));
	const Motion *pmotion = (Motion *) pplan;

	CDXLPhysicalMotion *pdxlopMotion = NULL;

	// extract plan properties
	CDXLPhysicalProperties *pdxlprop = PdxlpropFromPlan(pplan);

	GPOS_ASSERT(NULL != pdxlprop);

	// construct DXL motion operator
	GPOS_ASSERT(MOTIONTYPE_FIXED == pmotion->motionType ||
				MOTIONTYPE_HASH == pmotion->motionType);

	if (MOTIONTYPE_FIXED == pmotion->motionType)
	{
		if (0 == pmotion->numOutputSegs)
		{
			pdxlopMotion = New(m_pmp) CDXLPhysicalBroadcastMotion(m_pmp);
		}
		else
		{
			pdxlopMotion = New(m_pmp) CDXLPhysicalGatherMotion(m_pmp);
		}
	}
	else
	{
		pdxlopMotion = New(m_pmp) CDXLPhysicalRedistributeMotion(m_pmp, false /*fDuplicateSensitive*/);
	}

	DrgPi *pdrgpiInputSegIds = New(m_pmp) DrgPi(m_pmp);
	DrgPi *pdrgpiOutputSegIds = New(m_pmp) DrgPi(m_pmp);

	TranslateMotionSegmentInfo(pmotion, pdrgpiInputSegIds, pdrgpiOutputSegIds);

	pdxlopMotion->SetInputSegIds(pdrgpiInputSegIds);
	pdxlopMotion->SetOutputSegIds(pdrgpiOutputSegIds);

	// construct motion operator node
	CDXLNode *pdxln = New(m_pmp) CDXLNode(m_pmp, pdxlopMotion);
	pdxln->SetProperties(pdxlprop);

	// translate child of motion node
	Plan *pplanChild = (pmotion->plan).lefttree;

	GPOS_ASSERT(NULL != pplanChild);

	CDXLNode *pdxlnChild = PdxlnFromPlan(pplanChild);

	GPOS_ASSERT(1 <= pdxlnChild->UlArity());

	// get child projection lists
	CDXLNode *pdxlnPrLChild = (*pdxlnChild)[0];

	GPOS_ASSERT(pdxlnPrLChild->Pdxlop()->Edxlop() == EdxlopScalarProjectList);

	// construct projection list and filter
	CDXLNode *pdxlnPrL = NULL;
	CDXLNode *pdxlnFilter = NULL;

	m_psctranslator->SetCallingPhysicalOpType(EpspotMotion);

	TranslateTListAndQual(
		(pmotion->plan).targetlist,
		(pmotion->plan).qual,
		0,				//ulRTEIndex
		NULL, 			//pdxltabdesc
		NULL,			//pmdindex
		pdxlnPrLChild,
		NULL,			//pdxlnPrLRight
		&pdxlnPrL,
		&pdxlnFilter
		);

	// construct sorting column info
	CDXLNode *pdxlnSortColList = PdxlnSortingColListFromPlan(pmotion->sortColIdx, pmotion->sortOperators, pmotion->numSortCols, pdxlnPrL);

	// add children in the right order
	pdxln->AddChild(pdxlnPrL);			// proj list
	pdxln->AddChild(pdxlnFilter);		// filter
	pdxln->AddChild(pdxlnSortColList);	// sorting column list

	if (pdxlopMotion->Edxlop() == EdxlopPhysicalMotionRedistribute)
	{
		// redistribute is a motion: translate hash expr list
		CDXLNode *pdxlnHashExprList = PdxlnHashExprLFromList
										(
										pmotion->hashExpr,
										pmotion->hashDataTypes,
										pdxlnPrLChild
										);

		pdxln->AddChild(pdxlnHashExprList);
	}

	pdxln->AddChild(pdxlnChild);		// child

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnAggFromPlan
//
//	@doc:
//		Create a DXL aggregate node from a GPDB Agg plan node.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnAggFromPlan
	(
	const Plan *pplan
	)
{
	GPOS_ASSERT(NULL != pplan);
	GPOS_ASSERT(IsA(pplan, Agg));
	const Agg *pagg = (Agg *) pplan;

	// extract plan properties
	CDXLPhysicalProperties *pdxlprop = PdxlpropFromPlan(pplan);

	GPOS_ASSERT(NULL != pdxlprop);

	// construct DXL aggregate operator
	EdxlAggStrategy edxlaggstr = EdxlaggstrategySentinel;
	switch (pagg->aggstrategy)
	{
		case AGG_PLAIN:
			edxlaggstr = EdxlaggstrategyPlain;
			break;
		case AGG_SORTED:
			edxlaggstr = EdxlaggstrategySorted;
			break;
		case AGG_HASHED:
			edxlaggstr = EdxlaggstrategyHashed;
			break;
		default:
			GPOS_ASSERT(!"Invalid aggregation strategy");
	}

	BOOL fStreamSafe = pagg->streaming;
	CDXLPhysicalAgg *pdxlopAgg = New(m_pmp) CDXLPhysicalAgg(m_pmp, edxlaggstr, fStreamSafe);

	// translate child of aggregate operator
	Plan *pplanChild = (pagg->plan).lefttree;
	GPOS_ASSERT(NULL != pplanChild);
	CDXLNode *pdxlnChild = PdxlnFromPlan(pplanChild);

#ifdef GPOS_DEBUG
	pdxlnChild->Pdxlop()->AssertValid(pdxlnChild, false /* fValidateChildren */);
#endif

	// get child projection lists
	CDXLNode *pdxlnPrLChild = (*pdxlnChild)[0];

	// construct projection list and filter
	CDXLNode *pdxlnPrL = NULL;
	CDXLNode *pdxlnFilter = NULL;

	m_psctranslator->SetCallingPhysicalOpType(EpspotAgg);

	TranslateTListAndQual(
		(pagg->plan).targetlist,
		(pagg->plan).qual,
		0,				//ulRTEIndex
		NULL, 			//pdxltabdesc
		NULL,			//pmdindex
		pdxlnPrLChild,
		NULL,			//pdxlnPrLRight
		&pdxlnPrL,
		&pdxlnFilter
		);

	// translate grouping column list
	GPOS_ASSERT( (0 == pagg->numCols && NULL == pagg->grpColIdx) ||
			     (0 < pagg->numCols && NULL != pagg->grpColIdx));

	// translate output segment cols
	DrgPul *pdrgpulGroupingCols = New(m_pmp) DrgPul(m_pmp);

	for(ULONG ul = 0; ul < (ULONG) pagg->numCols; ul++)
	{
		ULONG ulColIdx = pagg->grpColIdx[ul];

		// find grouping column in the project list of the child node
		CDXLNode *pdxlnGroupingCol = (*pdxlnPrLChild)[ulColIdx-1];
		CDXLScalarProjElem *pdxlopPrEl = (CDXLScalarProjElem *) pdxlnGroupingCol->Pdxlop();

		ULONG *pulGroupingColId = New(m_pmp) ULONG(pdxlopPrEl->UlId());
		pdrgpulGroupingCols->Append(pulGroupingColId);
	}

	pdxlopAgg->SetGroupingCols(pdrgpulGroupingCols);

	// construct aggregate operator node
	CDXLNode *pdxln = New(m_pmp) CDXLNode(m_pmp, pdxlopAgg);
	pdxln->SetProperties(pdxlprop);

	// add children in the right order
	pdxln->AddChild(pdxlnPrL);		// proj list
	pdxln->AddChild(pdxlnFilter);	// filter
	pdxln->AddChild(pdxlnChild);	// child

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnWindowFromPlan
//
//	@doc:
//		Create a DXL window node from a GPDB window plan node.
//		The function allocates memory in the translator memory pool,
//		and the caller is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnWindowFromPlan
	(
	const Plan *pplan
	)
{
	GPOS_ASSERT(NULL != pplan);
	GPOS_ASSERT(IsA(pplan, Window));
	const Window *pwindow = (Window *) pplan;

	// translate child of window operator
	Plan *pplanChild = (pwindow->plan).lefttree;
	GPOS_ASSERT(NULL != pplanChild);
	CDXLNode *pdxlnChild = PdxlnFromPlan(pplanChild);

#ifdef GPOS_DEBUG
	pdxlnChild->Pdxlop()->AssertValid(pdxlnChild, false /* fValidateChildren */);
#endif

	// get child projection lists
	CDXLNode *pdxlnPrLChild = (*pdxlnChild)[0];

	// construct projection list and filter
	CDXLNode *pdxlnPrL = NULL;
	CDXLNode *pdxlnFilter = NULL;

	m_psctranslator->SetCallingPhysicalOpType(EpspotWindow);

	TranslateTListAndQual
		(
		(pwindow->plan).targetlist,
		(pwindow->plan).qual,
		0,				//ulRTEIndex
		NULL, 			//pdxltabdesc
		NULL,			//pmdindex
		pdxlnPrLChild,
		NULL,			//pdxlnPrLRight
		&pdxlnPrL,
		&pdxlnFilter
		);

	const ULONG ulSize = (ULONG) pwindow->numPartCols;
	// translate partition columns
	DrgPul *pdrgpulPartCols = New(m_pmp) DrgPul(m_pmp);
	for(ULONG ul = 0; ul < ulSize; ul++)
	{
		ULONG ulColIdx = (ULONG) pwindow->partColIdx[ul];

		// find partition column in the project list of the child node
		CDXLNode *pdxlnPartCol = (*pdxlnPrLChild)[ulColIdx-1];
		CDXLScalarProjElem *pdxlopPrEl = (CDXLScalarProjElem *) pdxlnPartCol->Pdxlop();

		ULONG *pulPartColId = New(m_pmp) ULONG(pdxlopPrEl->UlId());
		pdrgpulPartCols->Append(pulPartColId);
	}

	// add the window keys
	DrgPdxlwk *pdrgpdxlwk = New(m_pmp) DrgPdxlwk(m_pmp);
	ListCell *plcWindowKey = NULL;
	ForEach (plcWindowKey, pwindow->windowKeys)
	{
		WindowKey *pwindowkey = (WindowKey *) lfirst(plcWindowKey);
		CDXLWindowKey *pdxlWindowKey = New(m_pmp) CDXLWindowKey(m_pmp);

		CDXLNode *pdxlnSortColList = PdxlnSortingColListFromPlan
										(
										pwindowkey->sortColIdx,
										pwindowkey->sortOperators,
										pwindowkey->numSortCols,
										pdxlnPrLChild
										);

		pdxlWindowKey->SetSortColList(pdxlnSortColList);

		if (NULL != pwindowkey->frame)
		{
			CMappingVarColId mapvarcolid(m_pmp);
			mapvarcolid.LoadProjectElements(DEFAULT_QUERY_LEVEL, OUTER, pdxlnPrLChild);

			CDXLWindowFrame *pdxlwf = m_psctranslator->Pdxlwf
															(
															(Expr *) pwindowkey->frame,
															&mapvarcolid,
															NULL
															);
			pdxlWindowKey->SetWindowFrame(pdxlwf);
		}
		pdrgpdxlwk->Append(pdxlWindowKey);
	}

	// construct window operator node
	CDXLPhysicalWindow *pdxlopWindow = New(m_pmp) CDXLPhysicalWindow(m_pmp, pdrgpulPartCols, pdrgpdxlwk);
	CDXLNode *pdxln = New(m_pmp) CDXLNode(m_pmp, pdxlopWindow);

	// extract plan properties
	CDXLPhysicalProperties *pdxlprop = PdxlpropFromPlan(pplan);
	GPOS_ASSERT(NULL != pdxlprop);
	pdxln->SetProperties(pdxlprop);

	// add children in the right order
	pdxln->AddChild(pdxlnPrL);		// proj list
	pdxln->AddChild(pdxlnFilter);	// filter
	pdxln->AddChild(pdxlnChild);	// child

	return pdxln;
}

//---------------------------------------------------------------------------
//      @function:
//              CPlStmtTranslator::PdxlnAggFromPlan
//
//      @doc:
//              Create a DXL Aggregate node from a GPDB Unique plan node.
//              We do not support a separate Unique node in DXL. Instead, we convert the
//              Unique operator into an Aggregate node, which groups on the unique columns.
//
//              The function allocates memory in the translator memory pool, and the caller
//              is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnUniqueFromPlan
	(
	const Plan *pplan
	)
{
     GPOS_ASSERT(NULL != pplan);
	GPOS_ASSERT(IsA(pplan, Unique));
	const Unique *punique = (Unique *) pplan;

	Agg *pagg = MakeNode(Agg);
	Plan *pplanAgg = &(pagg->plan);
	const Plan *pplanUnique = &(punique->plan);

	// copy basic plan properties
	pplanAgg->plan_node_id = pplanUnique->plan_node_id;
	pplanAgg->plan_parent_node_id = pplanUnique->plan_parent_node_id;

	pplanAgg->startup_cost = pplanUnique->startup_cost;
	pplanAgg->total_cost = pplanUnique->total_cost;
	pplanAgg->plan_rows = pplanUnique->plan_rows;
	pplanAgg->plan_width = pplanUnique->plan_width;

	pplanAgg->nMotionNodes = pplanUnique->nMotionNodes;

	pplanAgg->targetlist = pplanUnique->targetlist;
	pplanAgg->qual = pplanUnique->qual;

	pplanAgg->lefttree = pplanUnique->lefttree;

	// copy unique-specific fields
	// execution of unique always assumes sorted input
	pagg->aggstrategy = AGG_SORTED;

	pagg->grpColIdx = punique->uniqColIdx;
	pagg->numCols = punique->numCols;

	return PdxlnAggFromPlan((Plan *)pagg);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnSortFromPlan
//
//	@doc:
//		Create a DXL sort node from a GPDB Sort plan node.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnSortFromPlan
	(
	const Plan *pplan
	)
{
 	GPOS_ASSERT(NULL != pplan);
	GPOS_ASSERT(IsA(pplan, Sort));
	const Sort *psort = (Sort *) pplan;

	// extract plan properties
	CDXLPhysicalProperties *pdxlprop = PdxlpropFromPlan(pplan);

	GPOS_ASSERT(NULL != pdxlprop);

	// construct DXL sort operator
	// TODO: Jan 19, 2011; currently GPDB supports only nullsLast behavior
	GPOS_ASSERT(NULL == psort->nullsFirst);

	CDXLPhysicalSort *pdxlopSort = New(m_pmp) CDXLPhysicalSort(m_pmp, psort->noduplicates);

	// translate child of sort operator
	Plan *pplanChild = (psort->plan).lefttree;
	GPOS_ASSERT(NULL != pplanChild);
	CDXLNode *pdxlnChild = PdxlnFromPlan(pplanChild);

#ifdef GPOS_DEBUG
	pdxlnChild->Pdxlop()->AssertValid(pdxlnChild, false /* fValidateChildren */);
#endif

	// get child projection lists
	CDXLNode *pdxlnPrLChild = (*pdxlnChild)[0];

	// construct projection list and filter
	CDXLNode *pdxlnPrL = NULL;
	CDXLNode *pdxlnFilter = NULL;

	m_psctranslator->SetCallingPhysicalOpType(EpspotSort);

	TranslateTListAndQual(
		(psort->plan).targetlist,
		(psort->plan).qual,
		0,				//ulRTEIndex
		NULL, 			//pdxltabdesc
		NULL,			//pmdindex
		pdxlnPrLChild,
		NULL,			//pdxlnPrLRight
		&pdxlnPrL,
		&pdxlnFilter
		);

	// translate sorting column list
	GPOS_ASSERT(0 < psort->numCols && NULL!= psort->sortColIdx);

	CDXLNode *pdxlnSortColList = PdxlnSortingColListFromPlan(psort->sortColIdx, psort->sortOperators, psort->numCols, pdxlnPrLChild);

	// translate limit information

	CDXLNode *pdxlnLimitCount = New(m_pmp) CDXLNode(m_pmp, New(m_pmp) CDXLScalarLimitCount(m_pmp));
	CDXLNode *pdxlnLimitOffset = New(m_pmp) CDXLNode(m_pmp, New(m_pmp) CDXLScalarLimitOffset(m_pmp));

	if (NULL != psort->limitCount)
	{
		CMappingVarColId mapvarcolid(m_pmp);
		mapvarcolid.LoadProjectElements(DEFAULT_QUERY_LEVEL, OUTER, pdxlnPrLChild);
		pdxlnLimitCount->AddChild(m_psctranslator->PdxlnScOpFromExpr
															(
															(Expr *) (psort->limitCount),
															&mapvarcolid
															)
								 );
	}

	if (NULL != psort->limitOffset)
	{
		CMappingVarColId mapvarcolid(m_pmp);
		mapvarcolid.LoadProjectElements(DEFAULT_QUERY_LEVEL, OUTER, pdxlnPrLChild);

		pdxlnLimitOffset->AddChild
							(
							m_psctranslator->PdxlnScOpFromExpr
												(
												(Expr *) (psort->limitOffset),
												&mapvarcolid
												)
							);
	}


	// construct sort operator node
	CDXLNode *pdxln = New(m_pmp) CDXLNode(m_pmp, pdxlopSort);
	pdxln->SetProperties(pdxlprop);

	// add children in the right order
	pdxln->AddChild(pdxlnPrL);			// proj list
	pdxln->AddChild(pdxlnFilter);		// filter
	pdxln->AddChild(pdxlnSortColList);	// sorting columns
	pdxln->AddChild(pdxlnLimitCount);	// limit count
	pdxln->AddChild(pdxlnLimitOffset);	// limit offset
	pdxln->AddChild(pdxlnChild);		// child

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnSubqueryScanFromPlan
//
//	@doc:
//		Create a DXL SubqueryScan node from a GPDB SubqueryScan plan node.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnSubqueryScanFromPlan
	(
	const Plan *pplan
	)
{
 	GPOS_ASSERT(NULL != pplan);
	GPOS_ASSERT(IsA(pplan, SubqueryScan));
	const SubqueryScan *psubqscan = (SubqueryScan *) pplan;

	// extract plan properties
	CDXLPhysicalProperties *pdxlprop = PdxlpropFromPlan(pplan);

	GPOS_ASSERT(NULL != pdxlprop);

	// get name for the subquery scan node from the range table
	RangeTblEntry *prte = (RangeTblEntry *) gpdb::PvListNth(m_pplstmt->rtable, (psubqscan->scan).scanrelid - 1);
	CHAR *szSubqName = prte->eref->aliasname;

	CWStringDynamic *pstrSubqName = CDXLUtils::PstrFromSz(m_pmp, szSubqName);
	// copy table name
	CMDName *pmdnameSubqScan = New(m_pmp) CMDName(m_pmp, pstrSubqName);
	delete pstrSubqName;

	// construct DXL subquery scan operator
	CDXLPhysicalSubqueryScan *pdxlopSubqScan = New(m_pmp) CDXLPhysicalSubqueryScan(m_pmp, pmdnameSubqScan);

	// translate child of subquery operator
	Plan *pplanChild = psubqscan->subplan;
	GPOS_ASSERT(NULL != pplanChild);
	CDXLNode *pdxlnChild = PdxlnFromPlan(pplanChild);

#ifdef GPOS_DEBUG
	pdxlnChild->Pdxlop()->AssertValid(pdxlnChild, false /* fValidateChildren */);
#endif

	// get child projection lists
	CDXLNode *pdxlnPrLChild = (*pdxlnChild)[0];

	// construct a fake table descriptor for the subquery scan
	// since the project list and filter refer to the rtable rather than OUTER
	// construct projection list and filter
	CDXLTableDescr *pdxltabdesc = New(m_pmp) CDXLTableDescr
											(
											m_pmp,
											New(m_pmp) CMDIdGPDB(CMDIdGPDB::m_mdidInvalidKey.OidObjectId()),
											New(m_pmp) CMDName(pmdnameSubqScan->Pstr()),
											prte->checkAsUser
											);

	ListCell *plcCol = NULL;
	ULONG ulAttno = 1;

	// add columns from RangeTableEntry to table descriptor
	ForEach (plcCol, prte->eref->colnames)
	{
		Value *pvalue = (Value *) lfirst(plcCol);
		CHAR *szColName = strVal(pvalue);

		BOOL fColDropped = false;

		if ('\0' == szColName[0])
		{
			fColDropped = true;
		}

		// column is not dropped
		CWStringDynamic *pstrColName = CDXLUtils::PstrFromSz(m_pmp, szColName);

		// copy string into column name
		CMDName *pmdnameColName = New(m_pmp) CMDName(m_pmp, pstrColName);

		delete pstrColName;

		// get column id from the subplan's target list
		CDXLNode *pdxlnPrEl = (*pdxlnPrLChild)[ulAttno-1];
		ULONG ulId = ((CDXLScalarProjElem *) pdxlnPrEl->Pdxlop())->UlId();

		// create a column descriptor for the column
		CDXLColDescr *pdxlcd = New(m_pmp) CDXLColDescr
											(
											m_pmp,
											pmdnameColName,
											ulId,
											ulAttno,
											New(m_pmp) CMDIdGPDB(CMDIdGPDB::m_mdidInvalidKey.OidObjectId()),
											fColDropped
											);

		pdxltabdesc->AddColumnDescr(pdxlcd);

		ulAttno++;
	}

	CDXLNode *pdxlnPrL = NULL;
	CDXLNode *pdxlnFilter = NULL;

	m_psctranslator->SetCallingPhysicalOpType(EpspotSubqueryScan);

	TranslateTListAndQual(
		(psubqscan->scan.plan).targetlist,
		(psubqscan->scan.plan).qual,
		ULONG((psubqscan->scan).scanrelid),
		pdxltabdesc,
		NULL,			// pmdindex
		NULL,			// pdxlnPrLChild
		NULL,			//pdxlnPrLRight
		&pdxlnPrL,
		&pdxlnFilter
		);

	// construct sort operator node
	CDXLNode *pdxln = New(m_pmp) CDXLNode(m_pmp, pdxlopSubqScan);
	pdxln->SetProperties(pdxlprop);

	// add children in the right order
	pdxln->AddChild(pdxlnPrL);			// proj list
	pdxln->AddChild(pdxlnFilter);		// filter
	pdxln->AddChild(pdxlnChild);		// child

	// cleanup
	pdxltabdesc->Release();

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnAppendFromPlan
//
//	@doc:
//		Create a DXL Append node from a GPDB Append plan node.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnAppendFromPlan
	(
	const Plan *pplan
	)
{
	GPOS_ASSERT(NULL != pplan);
	GPOS_ASSERT(IsA(pplan, Append));
	const Append *pappend = (Append *) pplan;

	GPOS_ASSERT(NULL != pappend->appendplans);

	// extract plan properties
	CDXLPhysicalProperties *pdxlprop = PdxlpropFromPlan(pplan);


	GPOS_ASSERT(NULL != pdxlprop);

	// construct DXL Append operator
	CDXLPhysicalAppend *pdxlopAppend = New(m_pmp) CDXLPhysicalAppend(m_pmp, pappend->isTarget, pappend->isZapped);

	CDXLNode *pdxln = New(m_pmp) CDXLNode(m_pmp, pdxlopAppend);
	pdxln->SetProperties(pdxlprop);

	// translate first child of append operator so we can translate the project list
	// and the filter
	Plan *pplanFirstChild = (Plan*) LInitial(pappend->appendplans);

	GPOS_ASSERT(NULL != pplanFirstChild);
	CDXLNode *pdxlnFirstChild = PdxlnFromPlan(pplanFirstChild);
#ifdef GPOS_DEBUG
	pdxlnFirstChild->Pdxlop()->AssertValid(pdxlnFirstChild, false /* fValidateChildren */);
#endif

	// get first child projection list
	CDXLNode *pdxlnPrLFirstChild = (*pdxlnFirstChild)[0];
	CDXLNode *pdxlnPrL = NULL;
	CDXLNode *pdxlnFilter = NULL;

	m_psctranslator->SetCallingPhysicalOpType(EpspotAppend);

	TranslateTListAndQual(
		(pappend->plan).targetlist,
		(pappend->plan).qual,
		0,				//ulRTEIndex
		NULL,			//table descr
		NULL,			//pmdindex
		pdxlnPrLFirstChild,
		NULL,				//pdxlnPrLRight
		&pdxlnPrL,
		&pdxlnFilter
		);

	// add children in the right order
	pdxln->AddChild(pdxlnPrL);			// proj list
	pdxln->AddChild(pdxlnFilter);		// filter
	pdxln->AddChild(pdxlnFirstChild);	// first child

	// translate the rest of the append child plans
	const ULONG ulLen = gpdb::UlListLength(pappend->appendplans);
	for (ULONG ul = 1; ul < ulLen; ul++)
	{
		Plan *pplanChild = (Plan*) gpdb::PvListNth(pappend->appendplans, ul);
		GPOS_ASSERT(NULL != pplanChild);
		CDXLNode *pdxlnChild = PdxlnFromPlan(pplanChild);
		pdxln->AddChild(pdxlnChild);
	}

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnResultFromFoldedFuncExpr
//
//	@doc:
//		Create a DXL physical Result node from a folded function expression
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnResultFromFoldedFuncExpr
	(
	const Expr *pexpr,
	CWStringDynamic *pstrAlias,
	CDXLPhysicalProperties *pdxlprop,
	CMappingVarColId *pmapvarcolid
	)
{
	CDXLNode *pdxlnPrL = New(m_pmp) CDXLNode(m_pmp, New(m_pmp) CDXLScalarProjList(m_pmp));

	// construct a scalar operator for the expression
	CDXLNode *pdxlnChild = m_psctranslator->PdxlnScOpFromExpr(pexpr, pmapvarcolid);

	ULONG ulPrElId = m_pidgtor->UlNextId();
	CMDName *pmdnameAlias = New(m_pmp) CMDName(m_pmp, pstrAlias);

	CDXLNode *pdxlnPrEl = New(m_pmp) CDXLNode
										(
										m_pmp,
										New(m_pmp) CDXLScalarProjElem(m_pmp, ulPrElId, pmdnameAlias)
										);
	pdxlnPrEl->AddChild(pdxlnChild);

	// add proj elem to proj list
	pdxlnPrL->AddChild(pdxlnPrEl);

	CDXLPhysicalResult *pdxlopResult = New(m_pmp) CDXLPhysicalResult(m_pmp);

	CDXLNode *pdxlnFilter = New(m_pmp) CDXLNode(m_pmp, New(m_pmp) CDXLScalarFilter(m_pmp));

	pmapvarcolid->LoadProjectElements(DEFAULT_QUERY_LEVEL, OUTER, pdxlnPrL);

	CDXLNode *pdxlnOneTimeFilter = New(m_pmp) CDXLNode(m_pmp, New(m_pmp) CDXLScalarOneTimeFilter(m_pmp));

	CDXLNode *pdxln = New(m_pmp) CDXLNode(m_pmp, pdxlopResult);
	pdxln->SetProperties(pdxlprop);

	pdxln->AddChild(pdxlnPrL);
	pdxln->AddChild(pdxlnFilter);
	pdxln->AddChild(pdxlnOneTimeFilter);

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnFunctionScanFromPlan
//
//	@doc:
//		Create a DXL physical TVF node from a GPDB function scan node.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnFunctionScanFromPlan
	(
	const Plan *pplan
	)
{
	GPOS_ASSERT(NULL != pplan);
	GPOS_ASSERT(IsA(pplan, FunctionScan));
	const FunctionScan *pfuncscan = (FunctionScan *) pplan;

	Index iRel = pfuncscan->scan.scanrelid;
	GPOS_ASSERT(0 < iRel);

	RangeTblEntry *prte = (RangeTblEntry *) gpdb::PvListNth(m_pplstmt->rtable, (iRel) - 1);

	GPOS_ASSERT(NULL != prte);
	GPOS_ASSERT(RTE_FUNCTION == prte->rtekind);

	Alias *palias = prte->eref;
	CWStringDynamic *pstrAlias = CDXLUtils::PstrFromSz(m_pmp, palias->aliasname);

	// get properties
	CDXLPhysicalProperties *pdxlprop = PdxlpropFromPlan(pplan);

	CMappingVarColId mapvarcolid(m_pmp);

	if (!IsA(prte->funcexpr, FuncExpr))
	{
		// for folded function expressions, construct a result node
		CDXLNode *pdxln = PdxlnResultFromFoldedFuncExpr
							(
							(Expr *)prte->funcexpr,
							pstrAlias,
							pdxlprop,
							&mapvarcolid
							);
		delete pstrAlias;

		return pdxln;
	}

	FuncExpr *pfuncexpr = (FuncExpr *) prte->funcexpr;
	CMDIdGPDB *pmdidFunc = New(m_pmp) CMDIdGPDB(pfuncexpr->funcid);
	CMDIdGPDB *pmdidRetType = New(m_pmp) CMDIdGPDB(pfuncexpr->funcresulttype);

	CWStringConst *pstrFunc = New(m_pmp) CWStringConst(m_pmp, pstrAlias->Wsz());
	delete pstrAlias;
	CDXLPhysicalTVF *pdxlop = New(m_pmp) CDXLPhysicalTVF(m_pmp, pmdidFunc, pmdidRetType, pstrFunc);

	CDXLNode *pdxlnTVF = New(m_pmp) CDXLNode(m_pmp, pdxlop);
	// add properties
	pdxlnTVF->SetProperties(pdxlprop);

	// construct and add projection list
	mapvarcolid.Load(DEFAULT_QUERY_LEVEL, iRel, m_pidgtor, palias->colnames);

	CDXLNode *pdxlnPrL = PdxlnPrLFromTL(pfuncscan->scan.plan.targetlist, &mapvarcolid);

	pdxlnTVF->AddChild(pdxlnPrL);

	// translate scalar children
	ListCell *plcArg = NULL;

	ForEach (plcArg, pfuncexpr->args)
	{
		Expr *pexpr = (Expr *) lfirst(plcArg);

		CDXLNode *pdxlnChild = m_psctranslator->PdxlnScOpFromExpr
									(
									pexpr,
									&mapvarcolid
									);

		pdxlnTVF->AddChild(pdxlnChild);
	}

	return pdxlnTVF;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnTblScanFromPlan
//
//	@doc:
//		Create a DXL table scan node from a GPDB sequential scan node.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnTblScanFromPlan
	(
	const Plan *pplan
	)
{
	GPOS_ASSERT(NULL != pplan);
	GPOS_ASSERT(IsA(pplan, SeqScan));
	const SeqScan *psscan = (SeqScan *) pplan;

	GPOS_ASSERT(0 < psscan->scanrelid);

	// construct table descriptor for the scan node from the range table entry
	RangeTblEntry *prte = (RangeTblEntry *) gpdb::PvListNth(m_pplstmt->rtable, (psscan->scanrelid) - 1);

	GPOS_ASSERT(NULL != prte);
	GPOS_ASSERT(RTE_RELATION == prte->rtekind);

	CDXLTableDescr *pdxltabdesc = CTranslatorUtils::Pdxltabdesc(m_pmp, m_pmda, m_pidgtor, prte);
	// get plan costs
	CDXLPhysicalProperties *pdxlprop = PdxlpropFromPlan(pplan);

	CDXLPhysicalTableScan *pdxlopTS = New(m_pmp) CDXLPhysicalTableScan(m_pmp, pdxltabdesc);

	CDXLNode *pdxlnTblScan = New(m_pmp) CDXLNode(m_pmp, pdxlopTS);
	pdxlnTblScan->SetProperties(pdxlprop);

	// construct projection list and filter
	CDXLNode *pdxlnPrL = NULL;
	CDXLNode *pdxlnFilter = NULL;

	m_psctranslator->SetCallingPhysicalOpType(EpspotTblScan);

	TranslateTListAndQual(
		(psscan->plan).targetlist,
		(psscan->plan).qual,
		ULONG(psscan->scanrelid),
		pdxltabdesc,
		NULL,	// pmdindex
		NULL, 	// pdxlnPrLLeft
		NULL,  	// pdxlnPrLRight
		&pdxlnPrL,
		&pdxlnFilter
		);

	// add children in the right order
	pdxlnTblScan->AddChild(pdxlnPrL);
	pdxlnTblScan->AddChild(pdxlnFilter);

	return pdxlnTblScan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnIndexScanFromPlan
//
//	@doc:
//		Create a DXL index scan node from a GPDB index scan node.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnIndexScanFromPlan
	(
	const Plan *pplan
	)
{
	GPOS_ASSERT(NULL != pplan);
	GPOS_ASSERT(IsA(pplan, IndexScan));
	const IndexScan *pindexscan = (IndexScan *) pplan;

	EPlStmtPhysicalOpType eplstmtphoptype = m_psctranslator->Eplsphoptype();
	m_psctranslator->SetCallingPhysicalOpType(EpspotIndexScan);

	CDXLNode *pdxln = PdxlnIndexScanFromGPDBIndexScan(pindexscan, false /*fIndexOnlyScan*/);

	m_psctranslator->SetCallingPhysicalOpType(eplstmtphoptype);

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnIndexScanFromGPDBIndexScan
//
//	@doc:
//		Create a DXL index (only) scan node from a GPDB index (only) scan node.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnIndexScanFromGPDBIndexScan
	(
	const IndexScan *pindexscan,
	BOOL fIndexOnlyScan
	)
{
	GPOS_ASSERT(0 < pindexscan->scan.scanrelid);
	GPOS_ASSERT(0 < pindexscan->indexid);

	// construct table descriptor for the scan node from the range table entry
	RangeTblEntry *prte =
			(RangeTblEntry *) gpdb::PvListNth(m_pplstmt->rtable, (pindexscan->scan.scanrelid) - 1);

	GPOS_ASSERT(NULL != prte);
	GPOS_ASSERT(RTE_RELATION == prte->rtekind);
	CDXLTableDescr *pdxltabdesc = CTranslatorUtils::Pdxltabdesc(m_pmp, m_pmda, m_pidgtor, prte);

	// create the index descriptor
	CMDIdGPDB *pmdid = CDXLUtils::Pmdid(m_pmp, pindexscan->indexid);
	CDXLIndexDescr *pdxlid = CTranslatorUtils::Pdxlid(m_pmp, m_pmda, pmdid);
	const IMDIndex *pmdindex = m_pmda->Pmdindex(pmdid);

	EdxlIndexScanDirection edxlissd = CTranslatorUtils::EdxlIndexDirection(pindexscan->indexorderdir);

	CDXLPhysicalIndexScan *pdxlop = NULL;

	GPOS_ASSERT(!fIndexOnlyScan);
	//{
	//	pdxlop = New(m_pmp) CDXLPhysicalIndexOnlyScan(m_pmp, pdxltabdesc, pdxlid, edxlissd);
	//}
	//else
	//{
		pdxlop = New(m_pmp) CDXLPhysicalIndexScan(m_pmp, pdxltabdesc, pdxlid, edxlissd);
	//}

	CDXLNode *pdxlnIndexScan = New(m_pmp) CDXLNode(m_pmp, pdxlop);

	// get plan costs
	CDXLPhysicalProperties *pdxlprop = PdxlpropFromPlan((Plan *)pindexscan);
	pdxlnIndexScan->SetProperties(pdxlprop);

	// translate the index condition
	CMappingVarColId mapvarcolidForIndex(m_pmp);

	if (NULL != pdxltabdesc)
	{
		mapvarcolidForIndex.LoadIndexColumns(DEFAULT_QUERY_LEVEL, ULONG(pindexscan->scan.scanrelid), pmdindex, pdxltabdesc);
	}

	// translate index condition
	CDXLNode *pdxlnIdxCondList = New(m_pmp) CDXLNode(m_pmp, New(m_pmp) CDXLScalarIndexCondList(m_pmp));

	ListCell *plcQual = NULL;
	ForEach (plcQual, pindexscan->indexqual)
	{
		Expr *pexpr = (Expr *) lfirst(plcQual);
		CDXLNode *pdxlnIdxQual = m_psctranslator->PdxlnScOpFromExpr(pexpr, &mapvarcolidForIndex);
		pdxlnIdxCondList->AddChild(pdxlnIdxQual);
	}

	// construct projection list and filter
	CDXLNode *pdxlnPrL = NULL;
	CDXLNode *pdxlnFilter = NULL;

	const IMDIndex *pmdindexProjList = NULL;
	if (fIndexOnlyScan)
	{
		pmdindexProjList = pmdindex;
	}

	TranslateTListAndQual
		(
		(pindexscan->scan.plan).targetlist,
		(pindexscan->scan.plan).qual,
		ULONG(pindexscan->scan.scanrelid),
		pdxltabdesc,
		pmdindexProjList,
		NULL, 	// pdxlnPrLLeft
		NULL,  	// pdxlnPrLRight
		&pdxlnPrL,
		&pdxlnFilter
		);

	// add children in the right order
	pdxlnIndexScan->AddChild(pdxlnPrL);
	pdxlnIndexScan->AddChild(pdxlnFilter);
	pdxlnIndexScan->AddChild(pdxlnIdxCondList);

	return pdxlnIndexScan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnLimitFromPlan
//
//	@doc:
//		Create a DXL Limit node from a GPDB LIMIT node.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnLimitFromPlan
	(
	const Plan *pplan
	)
{
	GPOS_ASSERT(NULL != pplan);
	GPOS_ASSERT(IsA(pplan, Limit));
	const Limit *plimit = (Limit *) pplan;

	// get plan costs
	CDXLPhysicalProperties *pdxlprop = PdxlpropFromPlan(pplan);

	CDXLPhysicalLimit *pdxlopLimit = New(m_pmp) CDXLPhysicalLimit(m_pmp);
	CDXLNode *pdxlnLimit = New(m_pmp) CDXLNode(m_pmp, pdxlopLimit);
	pdxlnLimit->SetProperties(pdxlprop);

	Plan *pplanChildPlan = outerPlan(plimit);

	GPOS_ASSERT(NULL != pplanChildPlan);
	CDXLNode *pdxlnChildPlan = PdxlnFromPlan(pplanChildPlan);
	GPOS_ASSERT(1 <= pdxlnChildPlan->UlArity());

	CDXLNode *pdxlnPrLLeft = (*pdxlnChildPlan)[0];
	GPOS_ASSERT(pdxlnPrLLeft->Pdxlop()->Edxlop() == EdxlopScalarProjectList);

	CMappingVarColId mapvarcolid(m_pmp);
	m_psctranslator->SetCallingPhysicalOpType(EpspotLimit);
	mapvarcolid.LoadProjectElements(DEFAULT_QUERY_LEVEL, OUTER, pdxlnPrLLeft);

	// construct projection list
	CDXLNode *pdxlnPrL = PdxlnPrLFromTL((plimit->plan).targetlist, &mapvarcolid);

	// add children in the right order
	pdxlnLimit->AddChild(pdxlnPrL); 		// project list
	pdxlnLimit->AddChild(pdxlnChildPlan);

	CDXLScalarLimitCount *pdxlopLimitCount = New(m_pmp) CDXLScalarLimitCount(m_pmp);
	CDXLNode *pdxlnLimitCount = New(m_pmp) CDXLNode(m_pmp, pdxlopLimitCount);

	if(NULL != plimit->limitCount)
	{
		pdxlnLimitCount->AddChild(m_psctranslator->PdxlnScOpFromExpr
															(
															(Expr *) (plimit->limitCount),
															&mapvarcolid
															)
								 );
	}
	pdxlnLimit->AddChild(pdxlnLimitCount);

	CDXLScalarLimitOffset *pdxlopLimitOffset = New(m_pmp) CDXLScalarLimitOffset(m_pmp);
	CDXLNode *pdxlnLimitOffset = New(m_pmp) CDXLNode(m_pmp, pdxlopLimitOffset);

	if(NULL != plimit->limitOffset)
	{
		pdxlnLimitOffset->AddChild(m_psctranslator->PdxlnScOpFromExpr
															(
															(Expr *) (plimit->limitOffset),
															&mapvarcolid
															)
								 );
	}

	pdxlnLimit->AddChild(pdxlnLimitOffset);

	return pdxlnLimit;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnMaterializeFromPlan
//
//	@doc:
//		Create a DXL Materialize node from a GPDB MATERIAL node.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnMaterializeFromPlan
	(
	const Plan *pplan
	)
{
	GPOS_ASSERT(NULL != pplan);
	GPOS_ASSERT(IsA(pplan, Material));
	const Material *pmat = (Material *) pplan;
	// get plan costs
	CDXLPhysicalProperties *pdxlprop = PdxlpropFromPlan(pplan);
	CDXLPhysicalMaterialize *pdxlopMat = NULL;

	if (SHARE_NOTSHARED == pmat->share_type)
	{
		// materialize node is not shared
		pdxlopMat = New(m_pmp) CDXLPhysicalMaterialize(m_pmp, pmat->cdb_strict);
	}
	else
	{
		// create a shared materialize node
		pdxlopMat = New(m_pmp) CDXLPhysicalMaterialize(m_pmp, pmat->cdb_strict, pmat->share_id, pmat->driver_slice, pmat->nsharer_xslice);
	}

	CDXLNode *pdxlnMaterialize = New(m_pmp) CDXLNode(m_pmp, pdxlopMat);
	pdxlnMaterialize->SetProperties(pdxlprop);

	Plan *pplanChildPlan = outerPlan(pmat);
	GPOS_ASSERT(NULL != pplanChildPlan);
	CDXLNode *pdxlnChildPlan = PdxlnFromPlan(pplanChildPlan);
	GPOS_ASSERT(1 <= pdxlnChildPlan->UlArity());

	CDXLNode *pdxlnPrLLeft = (*pdxlnChildPlan)[0];
	GPOS_ASSERT(pdxlnPrLLeft->Pdxlop()->Edxlop() == EdxlopScalarProjectList);

	// construct projection list and filter
	CDXLNode *pdxlnPrL = NULL;
	CDXLNode *pdxlnFilter = NULL;

	m_psctranslator->SetCallingPhysicalOpType(EpspotMaterialize);

	TranslateTListAndQual(
		(pmat->plan).targetlist,
		(pmat->plan).qual,
		0,				//ulRTEIndex
		NULL,			// table descr
		NULL,			// pmdindex
		pdxlnPrLLeft, 	// pdxlnPrLLeft
		NULL,  			// pdxlnPrLRight
		&pdxlnPrL,
		&pdxlnFilter
		);

	pdxlnMaterialize->AddChild(pdxlnPrL);
	pdxlnMaterialize->AddChild(pdxlnFilter);
	pdxlnMaterialize->AddChild(pdxlnChildPlan);

	return pdxlnMaterialize;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnSharedScanFromPlan
//
//	@doc:
//		Create a DXL SharedScan node from a GPDB SHAREINPUTSCAN node.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnSharedScanFromPlan
	(
	const Plan *pplan
	)
{
 	GPOS_ASSERT(NULL != pplan);
	GPOS_ASSERT(IsA(pplan, ShareInputScan));
	const ShareInputScan *pshscan = (ShareInputScan *) pplan;

	// get plan costs
	CDXLPhysicalProperties *pdxlprop = PdxlpropFromPlan(pplan);
 	CDXLPhysicalSharedScan *pdxlopShScan = NULL;

	CDXLSpoolInfo *pspoolinfo = PspoolinfoFromSharedScan(pshscan);
	pdxlopShScan = New(m_pmp) CDXLPhysicalSharedScan(m_pmp, pspoolinfo);
	CDXLNode *pdxlnSharedScan = New(m_pmp) CDXLNode(m_pmp, pdxlopShScan);
	pdxlnSharedScan->SetProperties(pdxlprop);

	Plan *pplanChildPlan = outerPlan(pshscan);

	// project list of child operator
	const CDXLNode *pdxlnPrLChild = NULL;
	CDXLNode *pdxlnChildPlan = NULL;
	if (NULL != pplanChildPlan)
	{
		// translate shared scan child
		pdxlnChildPlan = PdxlnFromPlan(pplanChildPlan);
		GPOS_ASSERT(1 <= pdxlnChildPlan->UlArity());
		pdxlnPrLChild = (*pdxlnChildPlan)[0];
	}
	else
	{
		// input for shared scan node has already been translated elsewhere:
		// fetch the corresponding project list
		ULONG ulSharedScanId = (ULONG) pshscan->share_id;
		pdxlnPrLChild = m_phmuldxlnSharedScanProjLists->PtLookup(&ulSharedScanId);
	}

	GPOS_ASSERT(NULL != pdxlnPrLChild);
	GPOS_ASSERT(pdxlnPrLChild->Pdxlop()->Edxlop() == EdxlopScalarProjectList);

	// construct projection list and filter
	CDXLNode *pdxlnPrL = NULL;
	CDXLNode *pdxlnFilter = NULL;

	m_psctranslator->SetCallingPhysicalOpType(EpspotSharedScan);

	TranslateTListAndQual(
		(pshscan->plan).targetlist,
		(pshscan->plan).qual,
		0,				//ulRTEIndex
		NULL,			// table descr
		NULL,			// pmdindex
		pdxlnPrLChild, 	// pdxlnPrLLeft
		NULL,  			// pdxlnPrLRight
		&pdxlnPrL,
		&pdxlnFilter
		);

	if (NULL != pplanChildPlan)
	{
		// first encounter of this shared scan id: store the translated proj list
		ULONG *pulSharedScanId = New(m_pmp) ULONG(pshscan->share_id);

		pdxlnPrL->AddRef();
#ifdef GPOS_DEBUG
		BOOL fInserted =
#endif
		m_phmuldxlnSharedScanProjLists->FInsert(pulSharedScanId, pdxlnPrL);
		GPOS_ASSERT(fInserted);
	}

	// add children in the right order
	pdxlnSharedScan->AddChild(pdxlnPrL); 		// project list
	pdxlnSharedScan->AddChild(pdxlnFilter);		// filter

	if (NULL != pdxlnChildPlan)
	{
		pdxlnSharedScan->AddChild(pdxlnChildPlan);
	}

	return pdxlnSharedScan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnSequence
//
//	@doc:
//		Create a DXL Sequnce node from a GPDB Sequence plan node.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnSequence
	(
	const Plan *pplan
	)
{
	GPOS_ASSERT(NULL != pplan);
	GPOS_ASSERT(IsA(pplan, Sequence));
	const Sequence *psequence = (Sequence *) pplan;
	GPOS_ASSERT(NULL != psequence->subplans);

	// extract plan properties
	CDXLPhysicalProperties *pdxlprop = PdxlpropFromPlan(pplan);

	GPOS_ASSERT(NULL != pdxlprop);

	// construct DXL sequence operator
	CDXLPhysicalSequence *pdxlop = New(m_pmp) CDXLPhysicalSequence(m_pmp);

	CDXLNode *pdxln = New(m_pmp) CDXLNode(m_pmp, pdxlop);
	pdxln->SetProperties(pdxlprop);

	// translate last child of sequence operator so we can translate the project list
	// and the filter
	const ULONG ulSubplans = gpdb::UlListLength(psequence->subplans);

	GPOS_ASSERT(0 < ulSubplans);

	Plan *pplanLastChild = (Plan*) gpdb::PvListNth(psequence->subplans, ulSubplans - 1);

	GPOS_ASSERT(NULL != pplanLastChild);

	CDXLNode *pdxlnLastChild = PdxlnFromPlan(pplanLastChild);

	// get last child projection list
	CDXLNode *pdxlnPrLLastChild = (*pdxlnLastChild)[0];

	CMappingVarColId mapvarcolid(m_pmp);
	mapvarcolid.LoadProjectElements(DEFAULT_QUERY_LEVEL, OUTER, pdxlnPrLLastChild);

	// construct projection list
	CDXLNode *pdxlnPrL = PdxlnPrLFromTL((psequence->plan).targetlist, &mapvarcolid);

	// add children in the right order
	pdxln->AddChild(pdxlnPrL);			// proj list

	// translate the rest of the sequence subplans
	for (ULONG ul = 0; ul < ulSubplans - 1; ul++)
	{
		Plan *pplanChild = (Plan*) gpdb::PvListNth(psequence->subplans, ul);
		CDXLNode *pdxlnChild = PdxlnFromPlan(pplanChild);
		pdxln->AddChild(pdxlnChild);
	}

	pdxln->AddChild(pdxlnLastChild);	// last child

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnDynamicTableScan
//
//	@doc:
//		Create a DXL Dynamic Table Scan node from the corresponding GPDB plan node.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnDynamicTableScan
	(
	const Plan *pplan
	)
{
	GPOS_ASSERT(NULL != pplan);
	GPOS_ASSERT(IsA(pplan, DynamicTableScan));
	const DynamicTableScan *pdts = (DynamicTableScan *) pplan;

	// extract plan properties
	CDXLPhysicalProperties *pdxlprop = PdxlpropFromPlan(pplan);

	// construct DXL dynamic table scan operator

	// construct table descriptor for the scan node from the range table entry
	RangeTblEntry *prte = (RangeTblEntry *) gpdb::PvListNth(m_pplstmt->rtable, (pdts->scanrelid) - 1);

	GPOS_ASSERT(NULL != prte);
	GPOS_ASSERT(RTE_RELATION == prte->rtekind);

	CDXLTableDescr *pdxltabdesc = CTranslatorUtils::Pdxltabdesc(m_pmp, m_pmda, m_pidgtor, prte);

	CDXLPhysicalDynamicTableScan *pdxlop = New(m_pmp) CDXLPhysicalDynamicTableScan(m_pmp, pdxltabdesc, pdts->partIndex, pdts->partIndexPrintable);

	CDXLNode *pdxln = New(m_pmp) CDXLNode(m_pmp, pdxlop);
	pdxln->SetProperties(pdxlprop);

	// construct projection list

	CMappingVarColId mapvarcolid(m_pmp);
	mapvarcolid.LoadTblColumns(DEFAULT_QUERY_LEVEL, ULONG(pdts->scanrelid), pdxltabdesc);

	// construct projection list
	CDXLNode *pdxlnPrL = PdxlnPrLFromTL((pdts->plan).targetlist, &mapvarcolid);

	pdxln->AddChild(pdxlnPrL); 		// project list

	return pdxln;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::TranslateTListAndQual
//
//	@doc:
//		Translate GPDB translate target and qual lists into DXL project list and
//		filter, respectively.
//		This function can be used for constructing the projection list and filter of a
//		base node (e.g. a scan node), in which case the the table descriptor is
//		not null and is used for resolving the column references, or in an intermediate
//		plan nodes, where pdxlnprlLeft and pldxlnprlRight are the projection lists
//		for the left and right child, respectively.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//
//---------------------------------------------------------------------------
void
CTranslatorPlStmtToDXL::TranslateTListAndQual
	(
	List *plTargetList,
	List *plQual,
	ULONG ulRTEIndex,
	const CDXLTableDescr *pdxltabdesc,
	const IMDIndex *pmdindex,
	const CDXLNode *pdxlnPrLLeft,
	const CDXLNode *pdxlnPrLRight,
	CDXLNode **ppdxlnPrLOut,
	CDXLNode **ppdxlnFilterOut
	)
{
	GPOS_ASSERT_IMP(NULL != pmdindex, NULL != pdxltabdesc);

	CMappingVarColId mapvarcolid(m_pmp);
	if (NULL != pdxlnPrLLeft)
	{
		mapvarcolid.LoadProjectElements(DEFAULT_QUERY_LEVEL, OUTER, pdxlnPrLLeft);
	}

	if (NULL != pdxlnPrLRight)
	{
		mapvarcolid.LoadProjectElements(DEFAULT_QUERY_LEVEL, INNER, pdxlnPrLRight);
	}

	if (NULL != pdxltabdesc)
	{
		GPOS_ASSERT(ulRTEIndex > 0);
		if (NULL != pmdindex)
		{
			mapvarcolid.LoadIndexColumns(DEFAULT_QUERY_LEVEL, ulRTEIndex, pmdindex, pdxltabdesc);
		}
		else
		{
			mapvarcolid.LoadTblColumns(DEFAULT_QUERY_LEVEL, ulRTEIndex, pdxltabdesc);
		}
	}

	// construct projection list
	*ppdxlnPrLOut = PdxlnPrLFromTL(plTargetList, &mapvarcolid);
	*ppdxlnFilterOut = m_psctranslator->PdxlnFilterFromQual
								(
								plQual,
								&mapvarcolid,
								EdxlopScalarFilter
								);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnPrLFromTL
//
//	@doc:
//		Create a DXL projection list node table descriptor from a GPDB target list.
//		This function can be used for constructing the projection list of a
//		base node (e.g. a scan node), in which case the the table descriptor is
//		not null and is used for resolving the column references, or in an intermediate
//		plan nodes, where pdxlnprlLeft and pldxlnprlRight are the projection lists
//		for the left and right child, respectively.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnPrLFromTL
	(
	List *plTargetList,
	CMappingVarColId *pmapvarcolid
	)
{
	CDXLNode *pdxlnPrL = New(m_pmp) CDXLNode(m_pmp, New(m_pmp) CDXLScalarProjList(m_pmp));
	ListCell *plcte = NULL;

	// construct a proj list node for each entry in the target list
	ForEach (plcte, plTargetList)
	{
		TargetEntry *pte = (TargetEntry *) lfirst(plcte);
		GPOS_ASSERT(IsA(pte, TargetEntry));

		// construct a scalar operator for the the Var's expression
		CDXLNode *pdxlnChild = m_psctranslator->PdxlnScOpFromExpr(pte->expr, pmapvarcolid);
		// construct a projection element
		CDXLScalarProjElem *pdxlopPrEl = NULL;
		// get the id and alias for the proj elem
		ULONG ulPrElId;
		CMDName *pmdnameAlias = NULL;

		if (NULL != pte->resname)
		{
			CWStringDynamic *pstrAlias = CDXLUtils::PstrFromSz(m_pmp, pte->resname);
			pmdnameAlias = New(m_pmp) CMDName(m_pmp, pstrAlias);
			// CName constructor copies string
			delete pstrAlias;
		}

		if (IsA(pte->expr, Var))
		{
			// project elem is a a reference to a column - use the colref id
			GPOS_ASSERT(EdxlopScalarIdent == pdxlnChild->Pdxlop()->Edxlop());
			CDXLScalarIdent *pdxlopIdent = (CDXLScalarIdent *) pdxlnChild->Pdxlop();
			ulPrElId = pdxlopIdent->Pdxlcr()->UlID();

			if (NULL == pte->resname)
			{
				// no alias provided - create a copy of the original column name
				pmdnameAlias = New(m_pmp) CMDName
											(
											m_pmp,
											pdxlopIdent->Pdxlcr()->Pmdname()->Pstr()
											);
			}
		}
		else
		{
			// project elem is a defined column - get a new id
			ulPrElId = m_pidgtor->UlNextId();
			// GPOS_ASSERT(NULL != pte->resname);
			if (NULL == pte->resname)
			{
				// no column name - make up one
				CWStringConst strUnnamedCol(GPOS_WSZ_LIT("?column?"));
				pmdnameAlias = New(m_pmp) CMDName(m_pmp, &strUnnamedCol);
			}
		}

		GPOS_ASSERT(NULL != pmdnameAlias);

		// construct a projection element operator
		pdxlopPrEl = New(m_pmp) CDXLScalarProjElem(m_pmp, ulPrElId, pmdnameAlias);

		// create the DXL node holding the proj elem
		CDXLNode *pdxlnPrEl = New(m_pmp) CDXLNode(m_pmp, pdxlopPrEl);
		pdxlnPrEl->AddChild(pdxlnChild);

		// add proj elem to proj list
		pdxlnPrL->AddChild(pdxlnPrEl);
	}

	return pdxlnPrL;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlpropFromPlan
//
//	@doc:
//		Extract cost estimates from Plan structures and store them in a
//		DXL properties container.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLPhysicalProperties *
CTranslatorPlStmtToDXL::PdxlpropFromPlan
	(
	const Plan *pplan
	)
{
	CWStringDynamic *pstrTotalCost = New(m_pmp) CWStringDynamic(m_pmp);
	CWStringDynamic *pstrStartupCost = New(m_pmp) CWStringDynamic(m_pmp);
	CWStringDynamic *pstrRows = New(m_pmp) CWStringDynamic(m_pmp);
	CWStringDynamic *pstrWidth = New(m_pmp) CWStringDynamic(m_pmp);

	const WCHAR wszFormat[] = GPOS_WSZ_LIT("%.2f");
	pstrTotalCost->AppendFormat(wszFormat, pplan->total_cost);
	pstrWidth->AppendFormat(GPOS_WSZ_LIT("%d"), pplan->plan_width);
	pstrStartupCost->AppendFormat(wszFormat, pplan->startup_cost);
	pstrRows->AppendFormat(wszFormat, pplan->plan_rows);

	CDXLOperatorCost *pdxlopcost = New(m_pmp) CDXLOperatorCost
												(
												pstrStartupCost,
												pstrTotalCost,
												pstrRows,
												pstrWidth
												);
	CDXLPhysicalProperties *pdxlprop = New(m_pmp) CDXLPhysicalProperties(pdxlopcost);

	return pdxlprop;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnHashExprLFromList
//
//	@doc:
//		Translate the list of hash expressions in a RedistributeMotion plan node.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnHashExprLFromList
	(
	List *plHashExpr,
	List *plHashExprTypes,
	const CDXLNode *pdxlnPrLChild
	)
{
	GPOS_ASSERT(NULL != plHashExpr);
	GPOS_ASSERT(NULL != plHashExprTypes);
	GPOS_ASSERT(gpdb::UlListLength(plHashExpr) == gpdb::UlListLength(plHashExprTypes));

	CDXLNode *pdxlnHashExprList = New(m_pmp) CDXLNode(m_pmp, New(m_pmp) CDXLScalarHashExprList(m_pmp));

	ListCell *plcHashExpr = NULL;
	ListCell *plcHashExprType = NULL;

	// construct a list of DXL hash expressions
	ForBoth (plcHashExpr, plHashExpr,
			plcHashExprType, plHashExprTypes)
	{
		Expr *pexpr = (Expr *) lfirst(plcHashExpr);
		OID oid = lfirst_oid(plcHashExprType);

		// construct a DXL hash expression node from the hash expression type id and the translated expression
		CDXLNode *pdxlnHashExpr = New(m_pmp) CDXLNode(m_pmp, New(m_pmp) CDXLScalarHashExpr(m_pmp, New(m_pmp) CMDIdGPDB(oid)));

		CMappingVarColId mapvarcolid(m_pmp);

		mapvarcolid.LoadProjectElements(DEFAULT_QUERY_LEVEL, OUTER, pdxlnPrLChild);

		// translate expression
		CDXLNode *pdxlnExpr = m_psctranslator->PdxlnScOpFromExpr(pexpr,&mapvarcolid);
		GPOS_ASSERT(NULL != pdxlnExpr);

		pdxlnHashExpr->AddChild(pdxlnExpr);
		pdxlnHashExprList->AddChild(pdxlnHashExpr);
	}

	return pdxlnHashExprList;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PdxlnSortingColListFromPlan
//
//	@doc:
//		Translate the list of hash expressions in a RedistributeMotion plan node.
//		The function allocates memory in the translator memory pool, and the caller
//		is responsible for freeing it.
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorPlStmtToDXL::PdxlnSortingColListFromPlan
	(
	AttrNumber *pattnoSortColIds,
	OID *poidSortOpIds,
	ULONG ulNumCols,
	const CDXLNode *pdxlnPrL
	)
{
	CDXLScalarSortColList *pdxlopSortColList = New(m_pmp) CDXLScalarSortColList(m_pmp);
	CDXLNode *pdxlnSortColList = New(m_pmp) CDXLNode(m_pmp, pdxlopSortColList);

	if (0 == ulNumCols)
	{
		// no sorting columns present - return the empty list
		return pdxlnSortColList;
	}

	GPOS_ASSERT(NULL != pattnoSortColIds && NULL != poidSortOpIds);

	// construct a list of DXL sorting columns
	for (ULONG ul = 0; ul < ulNumCols; ul++)
	{
		// find col id for the current sorting column
		AttrNumber attnoSortColId = pattnoSortColIds[ul];

		// find sorting column in the project list
		CDXLNode *pdxlnPrEl = (*pdxlnPrL)[attnoSortColId-1];
		CDXLScalarProjElem *pdxlopPrEl = (CDXLScalarProjElem *) pdxlnPrEl->Pdxlop();
		ULONG ulSortColId = pdxlopPrEl->UlId();

		// retrieve sorting operator oid and name
		OID oidSortOpId = poidSortOpIds[ul];

		CMDIdGPDB *pmdidSortOp = New(m_pmp) CMDIdGPDB(oidSortOpId);
		const IMDScalarOp *pmdscop = m_pmda->Pmdscop(pmdidSortOp);

		const CWStringConst *pstrSortOpName = pmdscop->Mdname().Pstr();;
		GPOS_ASSERT(NULL != pstrSortOpName);

		// TODO: Jan 19, 2011; read nullsFirst from the plan node;
		// currently GPDB does not support this
		CDXLScalarSortCol *pdxlopSortCol = New(m_pmp) CDXLScalarSortCol
													(
													m_pmp,
													ulSortColId,
													pmdidSortOp,
													New(m_pmp) CWStringConst(pstrSortOpName->Wsz()),
													false	// nullsFirst
													);

		CDXLNode *pdxlnSortCol = New(m_pmp) CDXLNode(m_pmp, pdxlopSortCol);
		pdxlnSortColList->AddChild(pdxlnSortCol);
	}

	return pdxlnSortColList;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::TranslateMotionSegmentInfo
//
//	@doc:
//		Populate the input and output segments id lists from a Motion node
//
//---------------------------------------------------------------------------
void
CTranslatorPlStmtToDXL::TranslateMotionSegmentInfo
	(
	const Motion *pmotion,
	DrgPi *pdrgpiInputSegIds,
	DrgPi *pdrgpiOutputSegIds
	)
{
	GPOS_ASSERT(NULL != pdrgpiInputSegIds);
	GPOS_ASSERT(NULL != pdrgpiOutputSegIds);

	const ULONG ulSegmentCount = gpdb::UlSegmentCountGP();

	// populate input segment ids
	Flow *pChildFlow = (pmotion->plan).lefttree->flow;

	if (pChildFlow->flotype == FLOW_SINGLETON)
	{
		// only one segment sends data
		INT *piInputSegId = New(m_pmp) INT(pChildFlow->segindex);
		pdrgpiInputSegIds->Append(piInputSegId);
	}
	else
	{
		// all segments send data
		for (ULONG ul = 0; ul < ulSegmentCount; ul++)
		{
			INT *piInputSegId = New(m_pmp) INT(ul);
			pdrgpiInputSegIds->Append(piInputSegId );
		}
	}

	// populate output segment ids
	if (pmotion->motionType == MOTIONTYPE_HASH)
	{
		// redistribute motion
		GPOS_ASSERT(0 < pmotion->numOutputSegs && NULL!= pmotion->outputSegIdx);

		for(ULONG ul = 0; ul < (ULONG) pmotion->numOutputSegs; ul++)
		{
			INT *piSegId = New(m_pmp) INT(pmotion->outputSegIdx[ul]);
			pdrgpiOutputSegIds->Append(piSegId);
		}
	}
	else if (pmotion->motionType == MOTIONTYPE_FIXED && pmotion->numOutputSegs > 0)
	{
		// gather motion
		GPOS_ASSERT(1 == pmotion->numOutputSegs);

		INT *piOutputSegId = New(m_pmp) INT(pmotion->outputSegIdx[0]);
		pdrgpiOutputSegIds->Append(piOutputSegId);
	}
	else
	{
		// broadcast motion
		for (ULONG ul = 0; ul < ulSegmentCount; ul++)
		{
			INT *piOutputSegId = New(m_pmp) INT(ul);
			pdrgpiOutputSegIds->Append(piOutputSegId);
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorPlStmtToDXL::PspoolinfoFromSharedScan
//
//	@doc:
//		Creates a spool info structure for a shared scan from a GPDB shared scan node
//
//---------------------------------------------------------------------------
CDXLSpoolInfo *
CTranslatorPlStmtToDXL::PspoolinfoFromSharedScan
	(
	const ShareInputScan *pshscan
	)
{
	GPOS_ASSERT(NULL != pshscan);
	GPOS_ASSERT(SHARE_NOTSHARED != pshscan->share_type);

	Edxlspooltype edxlsptype = EdxlspoolSentinel;

	switch (pshscan->share_type)
	{
		case SHARE_MATERIAL:
		case SHARE_MATERIAL_XSLICE:
			edxlsptype = EdxlspoolMaterialize;
			break;

		case SHARE_SORT:
		case SHARE_SORT_XSLICE:
			edxlsptype = EdxlspoolSort;
			break;

		default:
			GPOS_ASSERT(!"Invalid sharing type");
	}

	// is scan shared across slices
	BOOL fMultiSlice = false;

	if (SHARE_MATERIAL_XSLICE == pshscan->share_type || SHARE_SORT_XSLICE == pshscan->share_type)
	{
		fMultiSlice = true;
	}

	CDXLSpoolInfo *pspoolinfo = New(m_pmp) CDXLSpoolInfo(pshscan->share_id, edxlsptype, fMultiSlice, pshscan->driver_slice);
	return pspoolinfo;
}

// EOF

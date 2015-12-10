//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CTranslatorDXLToQuery.cpp
//
//	@doc:
//		Implementation of the methods used to translate a DXL tree into query.
//		All translator methods allocate memory in the provided memory pool, and
//		the caller is responsible for freeing it.
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "postgres.h"
#include "gpopt/translate/CMappingColIdVarQuery.h"
#include "gpopt/translate/CMappingElementColIdTE.h"
#include "gpopt/translate/CTranslatorDXLToQuery.h"
#include "gpopt/translate/CTranslatorDXLToPlStmt.h"
#include "gpopt/translate/CTranslatorUtils.h"

#include "gpos/base.h"
#include "gpos/common/CBitSet.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/md/IMDColumn.h"
#include "naucrates/md/IMDRelation.h"

#include "gpopt/mdcache/CMDAccessor.h"

#include "gpopt/gpdbwrappers.h"

using namespace gpmd;
using namespace gpdxl;
using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::CTranslatorDXLToQuery
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CTranslatorDXLToQuery::CTranslatorDXLToQuery
	(
	IMemoryPool *pmp,
	CMDAccessor *pmda,
	ULONG ulSegments
	)
	:
	m_pmp(pmp),
	m_pmda(pmda),
	m_ulSortgrouprefCounter(0),
	m_ulSegments(ulSegments)
{
	m_pdxlsctranslator = GPOS_NEW(m_pmp) CTranslatorDXLToScalar(m_pmp, m_pmda, m_ulSegments);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::~CTranslatorDXLToQuery
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CTranslatorDXLToQuery::~CTranslatorDXLToQuery()
{
	GPOS_DELETE(m_pdxlsctranslator);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::PqueryFromDXL
//
//	@doc:
//		Translate DXL node into a Query
//
//---------------------------------------------------------------------------
Query *
CTranslatorDXLToQuery::PqueryFromDXL
	(
	const CDXLNode *pdxln,
	const DrgPdxln *pdrgpdxlnQueryOutput,
	CStateDXLToQuery *pstatedxltoquery,
	TEMap *ptemap,
	ULONG ulQueryLevel
	)
{
	// initialize the colid->var mapping
	CMappingColIdVarQuery *pmapcidvarquery = GPOS_NEW(m_pmp) CMappingColIdVarQuery(m_pmp, ptemap, ulQueryLevel);

	GPOS_ASSERT(NULL != pdxln);

	Query *pquery = MakeNode(Query);

	TranslateLogicalOp(pdxln, pquery, pstatedxltoquery, pmapcidvarquery);

	if(0 == ulQueryLevel)
	{
		SetQueryOutput
			(
			pdrgpdxlnQueryOutput,
			pquery,
			pstatedxltoquery,
			pmapcidvarquery
			);
	}
	else
	{
		List *plTE = NIL;
		const ULONG ulSize = pstatedxltoquery->UlLength();
		for (ULONG ul = 0; ul < ulSize ; ul++)
		{
			TargetEntry *pte = pstatedxltoquery->PteColumn(ul);
			GPOS_ASSERT(NULL != pte);

			TargetEntry *pteCopy = (TargetEntry*) gpdb::PvCopyObject(pte);
			pteCopy->resno = (AttrNumber) (ul + 1);

			plTE = gpdb::PlAppendElement(plTE, pteCopy);
		}
		pquery->targetList = plTE;
	}

	// TODO: June 14 2011, We currently assume that all queries are of the type select.
	pquery->commandType = CMD_SELECT;

	GPOS_DELETE(pmapcidvarquery);

	if (m_pdxlsctranslator->FHasSubqueries())
	{
		pquery->hasSubLinks = true;
	}

	return pquery;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::PqueryFromDXLSubquery
//
//	@doc:
//		Translate DXL node subquery into a Query
//
//---------------------------------------------------------------------------
Query *
CTranslatorDXLToQuery::PqueryFromDXLSubquery
	(
	const CDXLNode *pdxln,
	ULONG ulColId,
	CStateDXLToQuery *pstatedxltoquery,
	TEMap *ptemap,
	ULONG ulQueryLevel
	)
{

	// initialize the colid->var mapping
	CMappingColIdVarQuery *pmapcidvarquery = GPOS_NEW (m_pmp) CMappingColIdVarQuery(m_pmp, ptemap, ulQueryLevel);

	GPOS_ASSERT(NULL != pdxln);

	Query *pquery = MakeNode(Query);

	TranslateLogicalOp(pdxln, pquery, pstatedxltoquery, pmapcidvarquery);


	SetSubqueryOutput
		(
		ulColId,
		pquery,
		pstatedxltoquery,
		pmapcidvarquery
		);

	pquery->commandType = CMD_SELECT;

	GPOS_DELETE(pmapcidvarquery);

	if (m_pdxlsctranslator->FHasSubqueries())
	{
		pquery->hasSubLinks = true;
	}

	return pquery;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::SetSubqueryOutput
//
//	@doc:
//		Set query target list based on the DXL query output
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToQuery::SetSubqueryOutput
	(
	ULONG ulColId,
	Query *pquery,
	CStateDXLToQuery *pstatedxltoquery,
	CMappingColIdVarQuery *pmapcidvarquery
	)
{
	GPOS_ASSERT(NULL != pquery);
	CStateDXLToQuery *pstatedxltoqueryOutput = GPOS_NEW(m_pmp) CStateDXLToQuery(m_pmp);

	List *plTE = NIL;
	ULONG ulResno = 1;
	const TargetEntry *pte = pmapcidvarquery->Pte(ulColId);

	TargetEntry *pteCopy =  (TargetEntry*) gpdb::PvCopyObject(const_cast<TargetEntry*>(pte));
	pteCopy->resno = (AttrNumber) ulResno;

	pstatedxltoqueryOutput->AddOutputColumnEntry(pteCopy, pteCopy->resname, ulColId);
	plTE = gpdb::PlAppendElement(plTE, pteCopy);

	const ULONG ulSize = pstatedxltoquery->UlLength();

	// Add grouping and ordering columns that are not in the query output in the
	// target list as resjunk entries.
	ULONG ulCounter = 0;

	for (ULONG ul = 0; ul < ulSize ; ul++)
	{
		TargetEntry *pteCurr = pstatedxltoquery->PteColumn(ul);
		GPOS_ASSERT(NULL != pteCurr);

		// we are only interested in grouping and ordering columns
		if(pteCurr->ressortgroupref > 0)
		{
			// check if we have already added the corresponding pte entry in pplTE
			BOOL fres = pstatedxltoqueryOutput->FTEFound(pteCurr);

			if(!fres)
			{
				ulResno++;
				// copy the entries
				TargetEntry *pteCopyCurr =  (TargetEntry*) gpdb::PvCopyObject(pteCurr);
				pteCopyCurr->resno = (AttrNumber) ulResno;
				pteCopyCurr->resjunk = true;

				pstatedxltoqueryOutput->AddOutputColumnEntry
											(
											pteCopyCurr,
											pteCopyCurr->resname,
											pstatedxltoquery->UlColId(ul)
											);
				plTE = gpdb::PlAppendElement(plTE, pteCopyCurr);
			}
		}
		ulCounter++;
	}

	pstatedxltoquery->Reload(pstatedxltoqueryOutput);
	GPOS_DELETE(pstatedxltoqueryOutput);

	pquery->targetList = plTE;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::SetQueryOutput
//
//	@doc:
//		Set query target list based on the DXL query output
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToQuery::SetQueryOutput
	(
	const DrgPdxln *pdrgpdxlnQueryOutput,
	Query *pquery,
	CStateDXLToQuery *pstatedxltoquery,
	CMappingColIdVarQuery *pmapcidvarquery
	)
{
	GPOS_ASSERT(NULL != pdrgpdxlnQueryOutput && NULL != pquery);

	CStateDXLToQuery *pstatedxltoqueryOutput = GPOS_NEW(m_pmp) CStateDXLToQuery(m_pmp);

	List *plTE = NIL;

	ULONG ulResno = 0;
	const ULONG ulLen = pdrgpdxlnQueryOutput->UlLength();
	// translate each DXL scalar ident into a target entry
	for (ULONG ul = 0; ul < ulLen; ul++)
	{
		CDXLNode *pdxlnIdent = (*pdrgpdxlnQueryOutput)[ul];
		CDXLScalarIdent *pdxlop = CDXLScalarIdent::PdxlopConvert(pdxlnIdent->Pdxlop());
		const CDXLColRef *pdxlcr = pdxlop->Pdxlcr();

		GPOS_ASSERT(NULL != pdxlcr);
		const ULONG ulColId = pdxlcr->UlID();
		const CMDName *pmdname = pdxlcr->Pmdname();
		const TargetEntry *pte = pmapcidvarquery->Pte(ulColId);

		ulResno++;
		TargetEntry *pteCopy =  (TargetEntry*) gpdb::PvCopyObject(const_cast<TargetEntry*>(pte));
		pteCopy->resname = CTranslatorUtils::SzFromWsz(pmdname->Pstr()->Wsz());
		pteCopy->resno = (AttrNumber) ulResno;

		pstatedxltoqueryOutput->AddOutputColumnEntry(pteCopy, pteCopy->resname, ulColId);
		plTE = gpdb::PlAppendElement(plTE, pteCopy);
	}

	const ULONG ulSize = pstatedxltoquery->UlLength();

	// Add grouping and ordering columns that are not in the query output in the
	// target list as resjunk entries.

	ULONG ulCounter = 0;

	for (ULONG ul = 0; ul < ulSize ; ul++)
	{
		TargetEntry *pte = pstatedxltoquery->PteColumn(ul);
		GPOS_ASSERT(NULL != pte);

		// we are only interested in grouping and ordering columns
		if(pte->ressortgroupref > 0)
		{
			// check if we have already added the corresponding pte entry in pplTE
			BOOL fres = pstatedxltoqueryOutput->FTEFound(pte);

			if(!fres)
			{
				ULONG ulColId = pstatedxltoquery->UlColId(ul);

				ulResno++;
				// copy the entries
				TargetEntry *pteCopy =  (TargetEntry*) gpdb::PvCopyObject(pte);
				pteCopy->resno = (AttrNumber) ulResno;
				pteCopy->resjunk = true;

				pstatedxltoqueryOutput->AddOutputColumnEntry(pteCopy, pteCopy->resname, ulColId);
				plTE = gpdb::PlAppendElement(plTE, pteCopy);
			}
		}
		ulCounter++;
	}

	pstatedxltoquery->Reload(pstatedxltoqueryOutput);
	GPOS_DELETE(pstatedxltoqueryOutput);

	pquery->targetList = plTE;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::TranslateLogicalOp
//
//	@doc:
//		Translates a DXL tree into a Query node
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToQuery::TranslateLogicalOp
	(
	const CDXLNode *pdxln,
	Query *pquery,
	CStateDXLToQuery *pstatedxltoquery,
	CMappingColIdVarQuery *pmapcidvarquery
	)
{
	STranslatorElem rgTranslators[] =
	{
		{EdxlopLogicalGet, &CTranslatorDXLToQuery::TranslateGet},
		{EdxlopLogicalProject, &CTranslatorDXLToQuery::TranslateProject},
		{EdxlopLogicalSelect, &CTranslatorDXLToQuery::TranslateSelect},
		{EdxlopLogicalJoin, &CTranslatorDXLToQuery::TranslateJoin},
		{EdxlopLogicalGrpBy, &CTranslatorDXLToQuery::TranslateGroupBy},
		{EdxlopLogicalLimit, &CTranslatorDXLToQuery::TranslateLimit},
		{EdxlopLogicalTVF, &CTranslatorDXLToQuery::TranslateTVF},
		{EdxlopLogicalSetOp, &CTranslatorDXLToQuery::TranslateSetOp},
	};

	const ULONG ulTranslators = GPOS_ARRAY_SIZE(rgTranslators);

	GPOS_ASSERT(NULL != pdxln);
	GPOS_ASSERT(NULL != pquery);
	Edxlopid eopid = pdxln->Pdxlop()->Edxlop();

	// find translator for the node type
	PfPexpr pf = NULL;
	for (ULONG ul = 0; ul < ulTranslators; ul++)
	{
		STranslatorElem elem = rgTranslators[ul];
		if (eopid == elem.eopid)
		{
			pf = elem.pf;
			break;
		}
	}

	if (NULL == pf)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnrecognizedOperator, pdxln->Pdxlop()->PstrOpName()->Wsz());
	}

	(this->*pf)(pdxln, pquery, pstatedxltoquery, pmapcidvarquery);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::TranslateGet
//
//	@doc:
//		Translate a logical get operator
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToQuery::TranslateGet
(
	const CDXLNode *pdxln,
	Query *pquery,
	CStateDXLToQuery *pstatedxltoquery,
	CMappingColIdVarQuery *pmapcidvarquery
)
{
	// This function must be called only for single table queries.
	// For multi-table queries, see TranslateDXLLgJoin

	RangeTblRef *prtref = PrtrefFromDXLLgGet(pdxln, pquery, pstatedxltoquery, pmapcidvarquery);

	GPOS_ASSERT(NULL == pquery->jointree);

	FromExpr *pfromexpr = MakeNode(FromExpr);
	pfromexpr->fromlist = NULL;
	pfromexpr->quals = NULL;
	pfromexpr->fromlist = gpdb::PlAppendElement(pfromexpr->fromlist, prtref);
	pquery->jointree =  pfromexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::TranslateTVF
//
//	@doc:
//		Translate a logical TVF operator
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToQuery::TranslateTVF
(
	const CDXLNode *pdxln,
	Query *pquery,
	CStateDXLToQuery *pstatedxltoquery,
	CMappingColIdVarQuery *pmapcidvarquery
)
{
	RangeTblRef *prtref = PrtrefFromDXLLgTVF(pdxln, pquery, pstatedxltoquery, pmapcidvarquery);

	GPOS_ASSERT(NULL == pquery->jointree);

	FromExpr *pfromexpr = MakeNode(FromExpr);
	pfromexpr->fromlist = NULL;
	pfromexpr->quals = NULL;
	pfromexpr->fromlist = gpdb::PlAppendElement(pfromexpr->fromlist, prtref);
	pquery->jointree =  pfromexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::TranslateSelect
//
//	@doc:
//		Translate a logical select operator
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToQuery::TranslateSelect
(
	const CDXLNode *pdxln,
	Query *pquery,
	CStateDXLToQuery *pstatedxltoquery,
	CMappingColIdVarQuery *pmapcidvarquery
)
{
	// translate children
	CDXLNode *pdxlnCond = (*pdxln)[0];
	CDXLNode *pdxlnChild = (*pdxln)[1];

	// creating a range table entry because we could have the condition on an aggregate or a computed column
	RangeTblRef *prtref = PrtrefFromDXLLgOp(pdxlnChild, pquery, pstatedxltoquery, pmapcidvarquery);
	FromExpr *pfromexpr = MakeNode(FromExpr);
	pfromexpr->fromlist = NULL;
	pfromexpr->quals = NULL;
	pfromexpr->fromlist = gpdb::PlAppendElement(pfromexpr->fromlist, prtref);
	pquery->jointree =  pfromexpr;

	Expr *pexpr = m_pdxlsctranslator->PexprFromDXLNodeScalar
			(
					pdxlnCond,
					pmapcidvarquery
			);
	GPOS_ASSERT(NULL == pquery->jointree->quals);

	pquery->jointree->quals = (Node*) pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::TranslateSetOp
//
//	@doc:
//		Translate a logical set operator
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToQuery::TranslateSetOp
	(
	const CDXLNode *pdxln,
	Query *pquery,
	CStateDXLToQuery *pstatedxltoquery,
	CMappingColIdVarQuery *pmapcidvarquery
	)
{
	// TODO: Oct 24th 2012, we currently assume during DXL->Query translations that
	// set ops are binary in nature which is not the case.
	GPOS_ASSERT(2 == pdxln->UlArity());

	CDXLLogicalSetOp *pdxlop = CDXLLogicalSetOp::PdxlopConvert(pdxln->Pdxlop());
	EdxlSetOpType edxlsetop = pdxlop->Edxlsetoptype();

	SetOperationStmt *psetop = MakeNode(SetOperationStmt);
	psetop->op = CTranslatorUtils::Setoptype(pdxlop->Edxlsetoptype());
	psetop->all = false;

	if (EdxlsetopUnionAll == edxlsetop ||
		EdxlsetopIntersectAll == edxlsetop ||
		EdxlsetopDifferenceAll == edxlsetop
	   )
	{
		psetop->all = true;
	}

	// translate children
	const ULONG ulArity = pdxln->UlArity();
	for (ULONG ul = 0; ul < ulArity; ul++)
	{
		CDXLNode *pdxlnChild = (*pdxln)[ul];

		CStateDXLToQuery *pstatedxltoqueryChild = GPOS_NEW(m_pmp) CStateDXLToQuery(m_pmp);
		RangeTblRef *prtrefChild = PrtrefFromDXLLgOp(pdxlnChild, pquery, pstatedxltoqueryChild, pmapcidvarquery);
		MarkUnusedColumns(pquery, prtrefChild, pstatedxltoqueryChild, pdxlop->Pdrgpul(ul) /*array of colids of the first child*/);
		GPOS_DELETE(pstatedxltoqueryChild);

		if (0 == ul)
		{
			psetop->larg = (Node*) prtrefChild;
		}
		else
		{
			psetop->rarg = (Node*) prtrefChild;
		}
	}

	// add the output columns to the translator state
	// add type information of the output columns to the set operator
	psetop->colTypes = NIL;
	psetop->colTypmods = NIL;

	const DrgPdxlcd *pdrgpdxlcd = pdxlop->Pdrgpdxlcd();
	const ULONG ulLen = pdrgpdxlcd->UlLength();
	for (ULONG ul = 0; ul < ulLen; ul++)
	{
		const CDXLColDescr *pdxlcd = (*pdrgpdxlcd)[ul];
		OID oid = CMDIdGPDB::PmdidConvert(pdxlcd->PmdidType())->OidObjectId();
		psetop->colTypes = gpdb::PlAppendOid(psetop->colTypes, oid);
		psetop->colTypmods = gpdb::PlAppendInt(psetop->colTypmods, -1);

		TargetEntry *pte = MakeNode(TargetEntry);
		Var *pvar = gpdb::PvarMakeVar(1, (AttrNumber) (ul + 1), oid, -1, 0);
		pte->expr = (Expr*) pvar;
		pte->resname = CTranslatorUtils::SzFromWsz(pdxlcd->Pmdname()->Pstr()->Wsz());
		pte->resno = 1;

		pstatedxltoquery->AddOutputColumnEntry(pte, pte->resname, pdxlcd->UlID());
	}

	pquery->jointree = MakeNode(FromExpr); // GPDB expects a from expr clause
	pquery->setOperations = (Node *) psetop;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::MarkUnusedColumns
//
//	@doc:
//		Mark unused target list entries in the setop child
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToQuery::MarkUnusedColumns
	(
	Query *pquery,
	RangeTblRef *prtref,
	CStateDXLToQuery *pstatedxltoquery,
	const DrgPul *pdrgpulColids
	)
{
	const ULONG ulRTIndex = prtref->rtindex;
	RangeTblEntry *prte = (RangeTblEntry*) gpdb::PvListNth(pquery->rtable, ulRTIndex -1);

	GPOS_ASSERT(RTE_SUBQUERY == prte->rtekind);
	Query *pqueryDerTbl = prte->subquery;

	// maintain the list of used columns in a bit set
	CBitSet *pds = GPOS_NEW(m_pmp) CBitSet(m_pmp);
	const ULONG ulLen = pdrgpulColids->UlLength();
	for (ULONG ul = 0; ul < ulLen; ul++)
	{
		ULONG ulValue = *((*pdrgpulColids)[ul]);
		(void) pds->FExchangeSet(ulValue);
	}

	// Mark all columns that are not in the list of required input columns as being unused
	const ULONG ulSize = pstatedxltoquery->UlLength();
	for (ULONG ul = 0; ul < ulSize; ul++)
	{
		ULONG ulColId = pstatedxltoquery->UlColId(ul);
		BOOL fSet = pds->FBit(ulColId);

		if (!fSet)
		{
			// mark the column as unused in the query
			TargetEntry *pte2 = (TargetEntry*) gpdb::PvListNth(pqueryDerTbl->targetList, ul);
			pte2->resjunk = true;

			// mark the column as unused in the state
			TargetEntry *pte = pstatedxltoquery->PteColumn(ul);
			pte->resjunk = true;
		}
	}

	pds->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::TranslateGroupBy
//
//	@doc:
//		Translate a logical groupby operator
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToQuery::TranslateGroupBy
(
	const CDXLNode *pdxln,
	Query *pquery,
	CStateDXLToQuery *pstatedxltoquery,
	CMappingColIdVarQuery *pmapcidvarquery
)
{
	CDXLOperator *pdxlop = pdxln->Pdxlop();
	CDXLLogicalGroupBy *pdxlnlggrpby = CDXLLogicalGroupBy::PdxlopConvert(pdxlop);

	// translate children
	CDXLNode *pdxlnProjectList = (*pdxln)[0];
	CDXLNode *pdxlnChild = (*pdxln)[1];

	RangeTblRef *prtref = PrtrefFromDXLLgOp(pdxlnChild, pquery, pstatedxltoquery, pmapcidvarquery);
	FromExpr *pfromexpr = MakeNode(FromExpr);
	pfromexpr->fromlist = NULL;
	pfromexpr->quals = NULL;
	pfromexpr->fromlist = gpdb::PlAppendElement(pfromexpr->fromlist, prtref);
	pquery->jointree =  pfromexpr;

	TranslateGroupByColumns(pdxlnlggrpby, pquery, pstatedxltoquery, pmapcidvarquery);
	TranslateProjList(pdxlnProjectList, pstatedxltoquery, pmapcidvarquery, pdxlnlggrpby->PdrgpulGroupingCols()->UlLength());
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::TranslateGroupByColumns
//
//	@doc:
//		Translate a logical group by columns
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToQuery::TranslateGroupByColumns
	(
	const CDXLLogicalGroupBy *pdxlnlggrpby,
	Query *pquery,
	CStateDXLToQuery *pstatedxltoquery,
	CMappingColIdVarQuery *pmapcidvarquery
	)
{
	pquery->hasAggs = true;
	List *plGrpCl = NIL;

	const DrgPul *pdrgpulGrpColId = pdxlnlggrpby->PdrgpulGroupingCols();

	// discard the previously inserted entries in the TE
	// as the query output will be composed of the grouping columns and the
	// project list defined in the group by operator
	pstatedxltoquery->Clear();

	if (NULL != pdrgpulGrpColId)
	{
		for (ULONG ul = 0; ul < pdrgpulGrpColId->UlLength(); ul++)
		{
			GPOS_ASSERT(NULL != (*pdrgpulGrpColId)[ul]);
			ULONG ulGroupingCol = *((*pdrgpulGrpColId)[ul]);

			GroupClause *pgrpcl = MakeNode(GroupClause);
			m_ulSortgrouprefCounter++;
			pgrpcl->tleSortGroupRef = m_ulSortgrouprefCounter;
			plGrpCl = gpdb::PlAppendElement(plGrpCl, pgrpcl);

			TargetEntry *pte = const_cast<TargetEntry *>(pmapcidvarquery->Pte(ulGroupingCol));

			OID oid = gpdb::OidExprType((Node*) pte->expr);
			CMDIdGPDB *pmdid = GPOS_NEW(m_pmp) CMDIdGPDB(oid);
			const IMDType *pmdtype = m_pmda->Pmdtype(pmdid);
			pmdid->Release();

			const CMDIdGPDB *pmdidSortOp = CMDIdGPDB::PmdidConvert(pmdtype->PmdidCmp(IMDType::EcmptL));
			pgrpcl->sortop = pmdidSortOp->OidObjectId();
			pgrpcl->nulls_first = false;

			GPOS_ASSERT(NULL != pte);

			pte->resno = (AttrNumber) (ul + 1);
			pte->ressortgroupref = pgrpcl->tleSortGroupRef;
			pstatedxltoquery->AddOutputColumnEntry(pte, pte->resname, ulGroupingCol);
		}
	}
	pquery->groupClause = plGrpCl;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::TranslateProject
//
//	@doc:
//		Translate a logical project operator
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToQuery::TranslateProject
(
	const CDXLNode *pdxln,
	Query *pquery,
	CStateDXLToQuery *pstatedxltoquery,
	CMappingColIdVarQuery *pmapcidvarquery
)
{
	// translate children
	CDXLNode *pdxlnProjectList = (*pdxln)[0];
	CDXLNode *pdxlnChild = (*pdxln)[1];

	RangeTblRef *prtref = PrtrefFromDXLLgOp(pdxlnChild, pquery, pstatedxltoquery, pmapcidvarquery);

	FromExpr *pfromexpr = MakeNode(FromExpr);
	pfromexpr->fromlist = NULL;
	pfromexpr->quals = NULL;
	pfromexpr->fromlist = gpdb::PlAppendElement(pfromexpr->fromlist, prtref);
	pquery->jointree =  pfromexpr;

	TranslateProjList(pdxlnProjectList, pstatedxltoquery, pmapcidvarquery, 0);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::TranslateJoin
//
//	@doc:
//		Translate a logical join operator
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToQuery::TranslateJoin
	(
	const CDXLNode *pdxln,
	Query *pquery,
	CStateDXLToQuery *pstatedxltoquery,
	CMappingColIdVarQuery *pmapcidvarquery
	)
{
	GPOS_ASSERT(NULL == pquery->jointree);

	ULONG ulChildCount = pdxln->UlArity();
	GPOS_ASSERT(2 < ulChildCount);

	FromExpr *pfromexpr = MakeNode(FromExpr);
	pfromexpr->fromlist = NULL;
	pfromexpr->quals = NULL;
	pquery->jointree = pfromexpr;

	if (3 == ulChildCount)
	{
		// Convert pdxln into a JoinExpr
		pfromexpr->fromlist = gpdb::PlAppendElement(pfromexpr->fromlist, PjoinexprFromDXLLgJoin(pdxln, pquery, pstatedxltoquery, pmapcidvarquery));
	}
	else
	{
		// An n-ary (where n > 2) join is:
		// 1. stored as a list of (RangeTableRef or JoinExpr) in pfromexpr->fromlist
		// 2. always of join type "inner" (All other join types are required to be a 2-way join)

		GPOS_ASSERT(EdxljtInner == CDXLLogicalJoin::PdxlopConvert(pdxln->Pdxlop())->Edxltype());

		ULONG ulResno=0;
		for (ULONG ulI = 0; ulI < ulChildCount-1; ++ulI)
		{
			CDXLNode *pdxlnChild = (*pdxln)[ulI];

			CStateDXLToQuery statedxltoqueryChild(m_pmp);

			pfromexpr->fromlist = gpdb::PlAppendElement(pfromexpr->fromlist, PnodeFromDXLLgJoinChild(pdxlnChild, pquery, &statedxltoqueryChild, pmapcidvarquery));

			ULONG ulSize = statedxltoqueryChild.UlLength();

			// insert alias names from the right child to its parent
			for(ULONG ulJ = 0; ulJ < ulSize; ulJ++)
			{
				ulResno++;
				TargetEntry *pte = statedxltoqueryChild.PteColumn(ulJ);
				CHAR *szColName = statedxltoqueryChild.SzColumnName(ulJ);
				GPOS_ASSERT(NULL != pte && NULL != szColName);

				TargetEntry *pteCopy =  (TargetEntry*) gpdb::PvCopyObject(pte);
				pteCopy->resno = (AttrNumber) ulResno;

				pstatedxltoquery->AddOutputColumnEntry(pteCopy, szColName, statedxltoqueryChild.UlColId(ulJ));
			}

		}

		// An n-ary (n > 2) with a where clause is represented as a: CDXLLogicalSelect on top of a CDXLLogicalJoin
		// The last child (CDXLScalar) will be CDXLScalarConstValue representing "true" to signify a cross product.

		GPOS_ASSERT(NULL != (*pdxln)[ulChildCount-1]);

		// translate scalar condition representing the joinqual
		CDXLNode *pdxlnCond = (*pdxln)[ulChildCount-1];

		Expr *pexpr = m_pdxlsctranslator->PexprFromDXLNodeScalar
							(
							pdxlnCond,
							pmapcidvarquery
							);

		pquery->jointree->quals = (Node*) pexpr;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::TranslateLimit
//
//	@doc:
//		Translate a logical limit operator
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToQuery::TranslateLimit
	(
	const CDXLNode *pdxln,
	Query *pquery,
	CStateDXLToQuery *pstatedxltoquery,
	CMappingColIdVarQuery *pmapcidvarquery
	)
{
	List *plSortCl = NIL;

	GPOS_ASSERT(4 == pdxln->UlArity());

	// get children
	CDXLNode *pdxlnSortColList = (*pdxln)[0];
	CDXLNode *pdxlnLimitCount = (*pdxln)[1];
	CDXLNode *pdxlnLimitOffset = (*pdxln)[2];
	CDXLNode *pdxlnChild = (*pdxln)[3];

	// translate child node
	TranslateLogicalOp(pdxlnChild, pquery, pstatedxltoquery, pmapcidvarquery);

	// translate sorting column lists
	const ULONG ulNumSortCols = pdxlnSortColList->UlArity();
	if (0 < ulNumSortCols)
	{
		for (ULONG ul = 0; ul < ulNumSortCols; ul++)
		{
			CDXLNode *pdxlnSortCol = (*pdxlnSortColList)[ul];
			CDXLScalarSortCol *pdxlopSortCol = CDXLScalarSortCol::PdxlopConvert(pdxlnSortCol->Pdxlop());

			// get the target entry and the set the sortgroupref
			ULONG ulSortColId = pdxlopSortCol->UlColId();
			TargetEntry *pte = const_cast<TargetEntry *>(pmapcidvarquery->Pte(ulSortColId));
			GPOS_ASSERT(NULL != pte);

			// create the sort clause
			SortClause *psortcl = MakeNode(SortClause);
			psortcl->sortop = CMDIdGPDB::PmdidConvert(pdxlopSortCol->PmdidSortOp())->OidObjectId();
			psortcl->nulls_first = pdxlopSortCol->FSortNullsFirst();

			plSortCl = gpdb::PlAppendElement(plSortCl, psortcl);

			// If ressortgroupref is not set then this column
			// was not used as a grouping column.
			if(0 == pte->ressortgroupref)
			{
				m_ulSortgrouprefCounter++;
				pte->ressortgroupref = m_ulSortgrouprefCounter;
			}

			psortcl->tleSortGroupRef = pte->ressortgroupref;

			if(!pstatedxltoquery->FTEFound(pte))
			{
				pstatedxltoquery->AddOutputColumnEntry(pte, pte->resname, ulSortColId);
			}
		}
	}

	GPOS_ASSERT(NULL != pdxlnLimitCount && NULL != pdxlnLimitOffset);

	// translate the limit count and offset;
	if(pdxlnLimitCount->UlArity() >0)
	{
		Expr *pexprCount = m_pdxlsctranslator->PexprFromDXLNodeScalar
												(
												(*pdxlnLimitCount)[0],
												pmapcidvarquery
												);

		pquery->limitCount = (Node *) pexprCount;
	}

	if(pdxlnLimitOffset->UlArity() >0)
	{
		Expr *pexprOffset = m_pdxlsctranslator->PexprFromDXLNodeScalar
												(
												(*pdxlnLimitOffset)[0],
												pmapcidvarquery
												);

		pquery->limitOffset = (Node *) pexprOffset;
	}

	pquery->sortClause = plSortCl;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::PrtrefFromDXLLgGet
//
//	@doc:
// 		Create a range table reference for a CDXLLogicalGet node
//
//---------------------------------------------------------------------------
RangeTblRef *
CTranslatorDXLToQuery::PrtrefFromDXLLgGet
	(
	const CDXLNode *pdxlnGet,
	Query *pquery,
	CStateDXLToQuery *pstatedxltoquery,
	CMappingColIdVarQuery *pmapcidvarquery
	)
{
	// For each CDXLLogicalGet node:
	// 1. Add an rtable entry to query->rtable
	// 2. Create an rtable reference to the newly generated rtable entry
	// 3. Return the rtref

	GPOS_ASSERT(NULL != pquery);

	// translate table descriptor into a range table entry
	CDXLLogicalGet *pdxlopGet = CDXLLogicalGet::PdxlopConvert(pdxlnGet->Pdxlop());

	// we will add the new range table entry as the last element of the range table
	Index iRel = gpdb::UlListLength(pquery->rtable) + 1;

	RangeTblEntry *prte = PrteFromTblDescr(pdxlopGet->Pdxltabdesc(), iRel, pstatedxltoquery, pmapcidvarquery);

	GPOS_ASSERT(NULL != prte);

	pquery->rtable = gpdb::PlAppendElement(pquery->rtable, prte);

	RangeTblRef *prtref = MakeNode(RangeTblRef);
	prtref->rtindex = iRel;
	return prtref;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::PnodeFromDXLLgJoinChild
//
//	@doc:
// 		Translate a child of a CDXLogicalJoin node into a GPDB Node
//
//---------------------------------------------------------------------------
Node *
CTranslatorDXLToQuery::PnodeFromDXLLgJoinChild
	(
	const CDXLNode *pdxlnChild,
	Query *pquery,
	CStateDXLToQuery *pstatedxltoquery,
	CMappingColIdVarQuery *pmapcidvarquery
	)
{
	Node *pnode = NULL;

	switch (pdxlnChild->Pdxlop()->Edxlop())
		{
			case EdxlopLogicalGet:
					pnode = (Node*) PrtrefFromDXLLgGet(pdxlnChild, pquery, pstatedxltoquery, pmapcidvarquery);
					break;
			case EdxlopLogicalJoin:
					pnode = (Node*) PjoinexprFromDXLLgJoin(pdxlnChild, pquery, pstatedxltoquery, pmapcidvarquery);
					break;
			case EdxlopLogicalTVF:
					pnode = (Node*) PrtrefFromDXLLgTVF(pdxlnChild, pquery, pstatedxltoquery, pmapcidvarquery);
					break;
			default:
					pnode = (Node*) PrtrefFromDXLLgOp(pdxlnChild, pquery, pstatedxltoquery, pmapcidvarquery);
					break;
		}
	return pnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::PrtrefFromDXLLgOp
//
//	@doc:
// 		Translate a CDXL Logical Node into derived table
//
//---------------------------------------------------------------------------
RangeTblRef *
CTranslatorDXLToQuery::PrtrefFromDXLLgOp
	(
	const CDXLNode *pdxln,
	Query *pquery,
	CStateDXLToQuery *pstatedxltoquery,
	CMappingColIdVarQuery *pmapcidvarquery
	)
{
	GPOS_ASSERT(EdxloptypeLogical == pdxln->Pdxlop()->Edxloperatortype());

	// initialize hash tables that maintains the mapping between ColId and TE and ColId and query level
	TEMap *ptemapDerTbl = CTranslatorUtils::PtemapCopy(m_pmp, pmapcidvarquery->Ptemap());

	CStateDXLToQuery statedxltoqueryDerived = CStateDXLToQuery(m_pmp);

	// translate the CDXLNode representing the derived table
	Query *pqueryDerTbl = PqueryFromDXL
							(
							pdxln,
							NULL, // NULL stands for the fact that we do not care about the output columns of derived table (This is not true for subqueries)
							&statedxltoqueryDerived,
							ptemapDerTbl,
							pmapcidvarquery->UlQueryLevel()+1
							);

	CRefCount::SafeRelease(ptemapDerTbl);

	// create a rtable entry for the derived table
	RangeTblEntry *prte = MakeNode(RangeTblEntry);

	prte->subquery = pqueryDerTbl;
	prte->rtekind = RTE_SUBQUERY;
	prte->inFromCl = false;
	pquery->rtable = gpdb::PlAppendElement(pquery->rtable, prte);

	ULONG ulRTIndex = gpdb::UlListLength(pquery->rtable);
	ULONG ulResno = 0;
	// create alias
	Alias *palias = MakeNode(Alias);


	ULONG ulDerivedOutputColumnSize = statedxltoqueryDerived.UlLength();

	// from each target list entry of the derived table
	// 1. create a target list entry and map col->TE at query current level
	// 2. insert the alias name in the joinaliasvars

	for(ULONG ul=0; ul<ulDerivedOutputColumnSize; ul++)
	{
		ulResno++;
		TargetEntry *pte = statedxltoqueryDerived.PteColumn(ul);
		CHAR *szColName = statedxltoqueryDerived.SzColumnName(ul);
		ULONG ulColId = statedxltoqueryDerived.UlColId(ul);

		if (!pte->resjunk)
		{
			TargetEntry *pteCopy = MakeNode(TargetEntry);
			Var *pvarNew = NULL;

			if (IsA(pte->expr, Var))
			{
				prte->joinaliasvars = gpdb::PlAppendElement(prte->joinaliasvars, pte->expr);
				Var *pvar = (Var*) pte->expr;

				pvarNew = gpdb::PvarMakeVar(ulRTIndex, (AttrNumber) ulResno, pvar->vartype, pvar->vartypmod, 0);
			}
			else
			{
				pvarNew = gpdb::PvarMakeVar(ulRTIndex, (AttrNumber) ulResno, gpdb::OidExprType( (Node*) pte->expr), -1, 0);
			}

			pteCopy->resno = (AttrNumber) ulResno;
			pteCopy->expr = (Expr*) pvarNew;
			pteCopy->resname = PStrDup(szColName);

			pmapcidvarquery->FInsertMapping(ulColId, pteCopy);
			palias->colnames = gpdb::PlAppendElement(palias->colnames, gpdb::PvalMakeString(szColName));
			pstatedxltoquery->AddOutputColumnEntry(pteCopy,  pteCopy->resname, ulColId);
		}
	}

	prte->alias = palias;
	prte->eref = palias;

	RangeTblRef *prtref = MakeNode(RangeTblRef);
	prtref->rtindex = ulRTIndex;

	return prtref;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::PjoinexprFromDXLLgJoin
//
//	@doc:
// 		Translate a Logical Join Node into a GPDB JoinExpr
//
//---------------------------------------------------------------------------
JoinExpr *
CTranslatorDXLToQuery::PjoinexprFromDXLLgJoin
	(
	const CDXLNode *pdxlnJoin,
	Query *pquery,
	CStateDXLToQuery *pstatedxltoquery,
	CMappingColIdVarQuery *pmapcidvarquery
	)
{
	GPOS_ASSERT(NULL != pquery && NULL != pdxlnJoin);
	const ULONG ulChildCount = pdxlnJoin->UlArity();
	GPOS_ASSERT(3 == ulChildCount);

	// create a joinexpr
	JoinExpr *pjoinexpr = MakeNode(JoinExpr);

	// translate the left child
	CDXLNode *pdxlnLeft = (*pdxlnJoin)[0];

	CStateDXLToQuery statedxltoqueryLeft = CStateDXLToQuery(m_pmp);
	pjoinexpr->larg = PnodeFromDXLLgJoinChild(pdxlnLeft, pquery, &statedxltoqueryLeft, pmapcidvarquery);

	// translate the right child
	CDXLNode *pdxlnRight = (*pdxlnJoin)[1];

	CStateDXLToQuery statedxltoqueryRight = CStateDXLToQuery(m_pmp);
	pjoinexpr->rarg = PnodeFromDXLLgJoinChild(pdxlnRight, pquery, &statedxltoqueryRight, pmapcidvarquery);

	// set the join type
	CDXLLogicalJoin *pdxlopJoin = CDXLLogicalJoin::PdxlopConvert(pdxlnJoin->Pdxlop());
	pjoinexpr->jointype = CTranslatorDXLToPlStmt::JtFromEdxljt(pdxlopJoin->Edxltype());

	// translate scalar condition representing the joinqual
	CDXLNode *pdxlnCond = (*pdxlnJoin)[ulChildCount-1];

	Expr *pexpr = m_pdxlsctranslator->PexprFromDXLNodeScalar
						(
						pdxlnCond,
						pmapcidvarquery
						);

	// Add the new range table entry for the join to the range table
	Index iRel = gpdb::UlListLength(pquery->rtable) + 1;
	pjoinexpr->rtindex = iRel;
	RangeTblEntry *prte = MakeNode(RangeTblEntry);

	prte->rtekind = RTE_JOIN;
	prte->jointype = pjoinexpr->jointype;
	prte->inFromCl = false;

	Alias *palias = MakeNode(Alias);
	ULONG ulResno = 0;
	const ULONG ulLeftOutputColumnSize = statedxltoqueryLeft.UlLength();
	// insert alias names from the left child to its parent
	for(ULONG ul = 0; ul < ulLeftOutputColumnSize; ul++)
	{
		TargetEntry *pte = statedxltoqueryLeft.PteColumn(ul);
		CHAR *szColName = statedxltoqueryLeft.SzColumnName(ul);

		ulResno++;
		TargetEntry *pteCopy =  (TargetEntry*) gpdb::PvCopyObject(pte);
		pteCopy->resno = (AttrNumber) ulResno;

		if(IsA(pteCopy->expr, Var))
		{
			prte->joinaliasvars = gpdb::PlAppendElement(prte->joinaliasvars, pteCopy->expr);
		}

		pstatedxltoquery->AddOutputColumnEntry(pteCopy, szColName, statedxltoqueryLeft.UlColId(ul));
		palias->colnames = gpdb::PlAppendElement(palias->colnames, gpdb::PvalMakeString(szColName));
	}

	const ULONG ulRightOutputColumnSize = statedxltoqueryRight.UlLength();

	// insert alias names from the right child to its parent
	for(ULONG ul = 0; ul < ulRightOutputColumnSize; ul++)
	{
		TargetEntry *pte = statedxltoqueryRight.PteColumn(ul);
		CHAR *szColName = statedxltoqueryRight.SzColumnName(ul);

		ulResno++;
		TargetEntry *pteCopy =  (TargetEntry*) gpdb::PvCopyObject(pte);
		pteCopy->resno = (AttrNumber) ulResno;

		if(IsA(pteCopy->expr, Var))
		{
			prte->joinaliasvars = gpdb::PlAppendElement(prte->joinaliasvars, pteCopy->expr);
		}

		pstatedxltoquery->AddOutputColumnEntry(pteCopy, szColName, statedxltoqueryRight.UlColId(ul));
		palias->colnames = gpdb::PlAppendElement(palias->colnames, gpdb::PvalMakeString(szColName));
	}

	prte->alias = palias;
	prte->eref = palias;

	pjoinexpr->quals = (Node*) pexpr;

	pquery->rtable = gpdb::PlAppendElement(pquery->rtable, prte);

	return pjoinexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::TranslateProjList
//
//	@doc:
//		Translates a DXL projection list
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToQuery::TranslateProjList
	(
	const CDXLNode *pdxlnPrL,
	CStateDXLToQuery *pstatedxltoquery,
	CMappingColIdVarQuery *pmapcidvarquery,
	ULONG ulTargetEntryIndex
	)
{
	if (NULL != pdxlnPrL)
	{
		// translate each DXL project element into a target entry
		const ULONG ulArity = pdxlnPrL->UlArity();
		for (ULONG ul = 0; ul < ulArity; ++ul)
		{
			CDXLNode *pdxlnPrEl = (*pdxlnPrL)[ul];
			CDXLScalarProjElem *pdxlopPrEl = CDXLScalarProjElem::PdxlopConvert(pdxlnPrEl->Pdxlop());

			GPOS_ASSERT(1 == pdxlnPrEl->UlArity());
			// translate proj element expression
			CDXLNode *pdxlnExpr = (*pdxlnPrEl)[0];

			Expr *pexpr = m_pdxlsctranslator->PexprFromDXLNodeScalar(pdxlnExpr, pmapcidvarquery);

			GPOS_ASSERT(NULL != pexpr);

			TargetEntry *pte = MakeNode(TargetEntry);
			pte->expr = pexpr;
			pte->resname = CTranslatorUtils::SzFromWsz(pdxlopPrEl->PmdnameAlias()->Pstr()->Wsz());
			pte->resno = (AttrNumber) (ulTargetEntryIndex + ul + 1);

			pstatedxltoquery->AddOutputColumnEntry(pte, pte->resname, pdxlopPrEl->UlId());
			//save mapping col id -> Var in the query translation context
			pmapcidvarquery->FInsertMapping(pdxlopPrEl->UlId(), pte);
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::PrteFromTblDescr
//
//	@doc:
//		Translates a DXL table descriptor into a range table entry
//
//---------------------------------------------------------------------------
RangeTblEntry *
CTranslatorDXLToQuery::PrteFromTblDescr
	(
	const CDXLTableDescr *pdxltabdesc,
	Index iRel,
	CStateDXLToQuery *pstatedxltoquery,
	CMappingColIdVarQuery *pmapcidvarquery
	)
{
	GPOS_ASSERT(0 == pstatedxltoquery->UlLength());

	RangeTblEntry *prte = MakeNode(RangeTblEntry);
	prte->rtekind = RTE_RELATION;

	// get oid for table
	CMDIdGPDB *pmdid = CMDIdGPDB::PmdidConvert(pdxltabdesc->Pmdid());
	prte->relid = pmdid->OidObjectId();

	Alias *palias = MakeNode(Alias);
	palias->colnames = NIL;

	// get table alias
	palias->aliasname = CTranslatorUtils::SzFromWsz(pdxltabdesc->Pmdname()->Pstr()->Wsz());

	// get column names
	const ULONG ulArity = pdxltabdesc->UlArity();
	for (ULONG ul = 0; ul < ulArity; ++ul)
	{
		const CDXLColDescr *pdxlcd = pdxltabdesc->Pdxlcd(ul);

		CHAR *szColName = CTranslatorUtils::SzFromWsz(pdxlcd->Pmdname()->Pstr()->Wsz());
		GPOS_ASSERT(NULL != pdxlcd);
		GPOS_ASSERT(0 != pdxlcd->IAttno());

		Value *pvalColName = gpdb::PvalMakeString(szColName);
		palias->colnames = gpdb::PlAppendElement(palias->colnames, pvalColName);

		const CMDIdGPDB *pmdidColType = CMDIdGPDB::PmdidConvert(pdxlcd->PmdidType());
		OID oidAttType = pmdidColType->OidObjectId();
		GPOS_ASSERT(InvalidOid != oidAttType);

		Var *pvar = gpdb::PvarMakeVar
						(
						iRel,
						(AttrNumber) pdxlcd->IAttno(),
						oidAttType,
						-1,	// vartypmod
						0
						);

		TargetEntry *pte = MakeNode(TargetEntry);
		pte->expr = (Expr*) pvar;
		pte->resname = szColName;
		pte->resno = (AttrNumber) pdxlcd->IAttno();

		//save mapping col id -> Var in the query translation context
		pmapcidvarquery->FInsertMapping(pdxlcd->UlID(), pte);

		pstatedxltoquery->AddOutputColumnEntry(pte, pte->resname, pdxlcd->UlID());
	}

	prte->eref = palias;

	return prte;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToQuery::PrtrefFromDXLLgTVF
//
//	@doc:
//		Create a range table reference for a CDXLLogicalTVF node
//
//---------------------------------------------------------------------------
RangeTblRef *
CTranslatorDXLToQuery::PrtrefFromDXLLgTVF
	(
	const CDXLNode *pdxlnTVF,
	Query *pquery,
	CStateDXLToQuery *pstatedxltoquery,
	CMappingColIdVarQuery *pmapcidvarquery
	)
{
	GPOS_ASSERT(0 == pstatedxltoquery->UlLength());

	Index iRel = gpdb::UlListLength(pquery->rtable) + 1;

	CDXLLogicalTVF *pdxlopTVF = CDXLLogicalTVF::PdxlopConvert(pdxlnTVF->Pdxlop());

	RangeTblEntry *prte = MakeNode(RangeTblEntry);
	prte->rtekind = RTE_FUNCTION;

	FuncExpr *pfuncexpr = MakeNode(FuncExpr);

	pfuncexpr->funcid = CMDIdGPDB::PmdidConvert(pdxlopTVF->PmdidFunc())->OidObjectId();
	pfuncexpr->funcretset = true;
	// this is a function call, as opposed to a cast
	pfuncexpr->funcformat = COERCE_EXPLICIT_CALL;
	pfuncexpr->funcresulttype = CMDIdGPDB::PmdidConvert(pdxlopTVF->PmdidRetType())->OidObjectId();

	Alias *palias = MakeNode(Alias);
	palias->colnames = NIL;

	// get function alias
	palias->aliasname = CTranslatorUtils::SzFromWsz(pdxlopTVF->Pmdname()->Pstr()->Wsz());

	// get column names and types
	const ULONG ulColumns = pdxlopTVF->UlArity();
	for (ULONG ul = 0; ul < ulColumns; ++ul)
	{
		const CDXLColDescr *pdxlcd = pdxlopTVF->Pdxlcd(ul);

		CHAR *szColName = CTranslatorUtils::SzFromWsz(pdxlcd->Pmdname()->Pstr()->Wsz());
		GPOS_ASSERT(NULL != pdxlcd);
		GPOS_ASSERT(0 != pdxlcd->IAttno());

		Value *pvalColName = gpdb::PvalMakeString(szColName);
		palias->colnames = gpdb::PlAppendElement(palias->colnames, pvalColName);

		const CMDIdGPDB *pmdidColType = CMDIdGPDB::PmdidConvert(pdxlcd->PmdidType());
		OID oidAttType = pmdidColType->OidObjectId();
		GPOS_ASSERT(InvalidOid != oidAttType);

		prte->funccoltypes = gpdb::PlAppendOid(prte->funccoltypes, oidAttType);
		prte->funccoltypmods = gpdb::PlAppendInt(prte->funccoltypmods, -1);

		Var *pvar = gpdb::PvarMakeVar
						(
						iRel,
						(AttrNumber) pdxlcd->IAttno(),
						oidAttType,
						-1,	// vartypmod
						0
						);

		TargetEntry *pte = MakeNode(TargetEntry);
		pte->expr = (Expr*) pvar;
		pte->resname = szColName;
		pte->resno = (AttrNumber) pdxlcd->IAttno();

		//save mapping col id -> Var in the query translation context
		pmapcidvarquery->FInsertMapping(pdxlcd->UlID(), pte);

		pstatedxltoquery->AddOutputColumnEntry(pte, pte->resname, pdxlcd->UlID());
	}

	// function arguments
	const ULONG ulArity = pdxlnTVF->UlArity();
	for (ULONG ul = 0; ul < ulArity; ++ul)
	{
		CDXLNode *pdxlnFuncArg = (*pdxlnTVF)[ul];
		Expr *pexprFuncArg = m_pdxlsctranslator->PexprFromDXLNodeScalar(pdxlnFuncArg, pmapcidvarquery);
		pfuncexpr->args = gpdb::PlAppendElement(pfuncexpr->args, pexprFuncArg);
	}

	prte->funcexpr = (Node *)pfuncexpr;
	prte->inFromCl = true;
	prte->eref = palias;

	pquery->rtable = gpdb::PlAppendElement(pquery->rtable, prte);

	RangeTblRef *prtref = MakeNode(RangeTblRef);
	prtref->rtindex = iRel;
	return prtref;
}

// EOF

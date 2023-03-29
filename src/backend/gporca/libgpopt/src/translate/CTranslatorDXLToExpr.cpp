//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CTranslatorDXLToExpr.cpp
//
//	@doc:
//		Implementation of the methods used to translate a DXL tree into Expr tree.
//		All translator methods allocate memory in the provided memory pool, and
//		the caller is responsible for freeing it.
//---------------------------------------------------------------------------

#include "gpopt/translate/CTranslatorDXLToExpr.h"

#include "gpos/common/CAutoTimer.h"

#include "gpopt/base/CAutoOptCtxt.h"
#include "gpopt/base/CColRef.h"
#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CColumnFactory.h"
#include "gpopt/base/CDistributionSpecAny.h"
#include "gpopt/base/CEnfdDistribution.h"
#include "gpopt/base/CEnfdOrder.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/exception.h"
#include "gpopt/mdcache/CMDAccessorUtils.h"
#include "gpopt/metadata/CColumnDescriptor.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CLogicalCTEAnchor.h"
#include "gpopt/operators/CLogicalCTEConsumer.h"
#include "gpopt/operators/CLogicalCTEProducer.h"
#include "gpopt/operators/CLogicalDelete.h"
#include "gpopt/operators/CLogicalDifference.h"
#include "gpopt/operators/CLogicalDifferenceAll.h"
#include "gpopt/operators/CLogicalDynamicGet.h"
#include "gpopt/operators/CLogicalForeignGet.h"
#include "gpopt/operators/CLogicalGbAgg.h"
#include "gpopt/operators/CLogicalGet.h"
#include "gpopt/operators/CLogicalInsert.h"
#include "gpopt/operators/CLogicalIntersect.h"
#include "gpopt/operators/CLogicalIntersectAll.h"
#include "gpopt/operators/CLogicalLimit.h"
#include "gpopt/operators/CLogicalProject.h"
#include "gpopt/operators/CLogicalSelect.h"
#include "gpopt/operators/CLogicalSequenceProject.h"
#include "gpopt/operators/CLogicalSetOp.h"
#include "gpopt/operators/CLogicalTVF.h"
#include "gpopt/operators/CLogicalUnion.h"
#include "gpopt/operators/CLogicalUnionAll.h"
#include "gpopt/operators/CLogicalUpdate.h"
#include "gpopt/operators/CScalarArray.h"
#include "gpopt/operators/CScalarArrayCoerceExpr.h"
#include "gpopt/operators/CScalarArrayRef.h"
#include "gpopt/operators/CScalarBooleanTest.h"
#include "gpopt/operators/CScalarCaseTest.h"
#include "gpopt/operators/CScalarCast.h"
#include "gpopt/operators/CScalarCoalesce.h"
#include "gpopt/operators/CScalarCoerceToDomain.h"
#include "gpopt/operators/CScalarCoerceViaIO.h"
#include "gpopt/operators/CScalarIdent.h"
#include "gpopt/operators/CScalarIf.h"
#include "gpopt/operators/CScalarIsDistinctFrom.h"
#include "gpopt/operators/CScalarMinMax.h"
#include "gpopt/operators/CScalarNullIf.h"
#include "gpopt/operators/CScalarNullTest.h"
#include "gpopt/operators/CScalarOp.h"
#include "gpopt/operators/CScalarProjectElement.h"
#include "gpopt/operators/CScalarProjectList.h"
#include "gpopt/operators/CScalarSortGroupClause.h"
#include "gpopt/operators/CScalarSubquery.h"
#include "gpopt/operators/CScalarSubqueryAll.h"
#include "gpopt/operators/CScalarSubqueryAny.h"
#include "gpopt/operators/CScalarSubqueryExists.h"
#include "gpopt/operators/CScalarSubqueryNotExists.h"
#include "gpopt/operators/CScalarSwitch.h"
#include "gpopt/operators/CScalarSwitchCase.h"
#include "gpopt/operators/CScalarValuesList.h"
#include "gpopt/translate/CTranslatorExprToDXLUtils.h"
#include "naucrates/dxl/operators/CDXLCtasStorageOptions.h"
#include "naucrates/dxl/operators/CDXLLogicalCTAS.h"
#include "naucrates/dxl/operators/CDXLLogicalCTEAnchor.h"
#include "naucrates/dxl/operators/CDXLLogicalCTEConsumer.h"
#include "naucrates/dxl/operators/CDXLLogicalCTEProducer.h"
#include "naucrates/dxl/operators/CDXLLogicalConstTable.h"
#include "naucrates/dxl/operators/CDXLLogicalDelete.h"
#include "naucrates/dxl/operators/CDXLLogicalGet.h"
#include "naucrates/dxl/operators/CDXLLogicalGroupBy.h"
#include "naucrates/dxl/operators/CDXLLogicalInsert.h"
#include "naucrates/dxl/operators/CDXLLogicalJoin.h"
#include "naucrates/dxl/operators/CDXLLogicalLimit.h"
#include "naucrates/dxl/operators/CDXLLogicalSetOp.h"
#include "naucrates/dxl/operators/CDXLLogicalTVF.h"
#include "naucrates/dxl/operators/CDXLLogicalUpdate.h"
#include "naucrates/dxl/operators/CDXLLogicalWindow.h"
#include "naucrates/dxl/operators/CDXLScalarAggref.h"
#include "naucrates/dxl/operators/CDXLScalarArray.h"
#include "naucrates/dxl/operators/CDXLScalarArrayCoerceExpr.h"
#include "naucrates/dxl/operators/CDXLScalarArrayComp.h"
#include "naucrates/dxl/operators/CDXLScalarArrayRef.h"
#include "naucrates/dxl/operators/CDXLScalarBooleanTest.h"
#include "naucrates/dxl/operators/CDXLScalarCaseTest.h"
#include "naucrates/dxl/operators/CDXLScalarCast.h"
#include "naucrates/dxl/operators/CDXLScalarCoalesce.h"
#include "naucrates/dxl/operators/CDXLScalarCoerceToDomain.h"
#include "naucrates/dxl/operators/CDXLScalarCoerceViaIO.h"
#include "naucrates/dxl/operators/CDXLScalarDistinctComp.h"
#include "naucrates/dxl/operators/CDXLScalarFuncExpr.h"
#include "naucrates/dxl/operators/CDXLScalarIdent.h"
#include "naucrates/dxl/operators/CDXLScalarIfStmt.h"
#include "naucrates/dxl/operators/CDXLScalarMinMax.h"
#include "naucrates/dxl/operators/CDXLScalarNullIf.h"
#include "naucrates/dxl/operators/CDXLScalarNullTest.h"
#include "naucrates/dxl/operators/CDXLScalarOpExpr.h"
#include "naucrates/dxl/operators/CDXLScalarProjElem.h"
#include "naucrates/dxl/operators/CDXLScalarSortCol.h"
#include "naucrates/dxl/operators/CDXLScalarSortGroupClause.h"
#include "naucrates/dxl/operators/CDXLScalarSubquery.h"
#include "naucrates/dxl/operators/CDXLScalarSubqueryQuantified.h"
#include "naucrates/dxl/operators/CDXLScalarSwitch.h"
#include "naucrates/exception.h"
#include "naucrates/md/CMDArrayCoerceCastGPDB.h"
#include "naucrates/md/CMDProviderMemory.h"
#include "naucrates/md/CMDRelationCtasGPDB.h"
#include "naucrates/md/IMDAggregate.h"
#include "naucrates/md/IMDCast.h"
#include "naucrates/md/IMDFunction.h"
#include "naucrates/md/IMDId.h"
#include "naucrates/md/IMDRelation.h"
#include "naucrates/md/IMDScalarOp.h"
#include "naucrates/traceflags/traceflags.h"

#define GPDB_DENSE_RANK_OID 7002
#define GPDB_PERCENT_RANK_OID 7003
#define GPDB_CUME_DIST_OID 7004
#define GPDB_NTILE_INT4_OID 7005
#define GPDB_NTILE_INT8_OID 7006
#define GPDB_NTILE_NUMERIC_OID 7007
#define GPOPT_ACTION_INSERT 0
#define GPOPT_ACTION_DELETE 1

using namespace gpos;
using namespace gpnaucrates;
using namespace gpmd;
using namespace gpdxl;
using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::CTranslatorDXLToExpr
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CTranslatorDXLToExpr::CTranslatorDXLToExpr(CMemoryPool *mp,
										   CMDAccessor *md_accessor,
										   BOOL fInitColumnFactory)
	: m_mp(mp),
	  m_sysid(IMDId::EmdidGeneral, GPMD_GPDB_SYSID),
	  m_pmda(md_accessor),
	  m_phmulcr(nullptr),
	  m_phmululCTE(nullptr),
	  m_pdrgpulOutputColRefs(nullptr),
	  m_pdrgpmdname(nullptr),
	  m_phmulpdxlnCTEProducer(nullptr),
	  m_ulCTEId(gpos::ulong_max),
	  m_pcf(nullptr)
{
	// initialize hash tables
	m_phmulcr = GPOS_NEW(m_mp) UlongToColRefMap(m_mp);

	// initialize hash tables
	m_phmululCTE = GPOS_NEW(m_mp) UlongToUlongMap(m_mp);

	if (fInitColumnFactory)
	{
		// get column factory from optimizer context object
		m_pcf = COptCtxt::PoctxtFromTLS()->Pcf();

		GPOS_ASSERT(nullptr != m_pcf);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::~CTranslatorDXLToExpr
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CTranslatorDXLToExpr::~CTranslatorDXLToExpr()
{
	m_phmulcr->Release();
	m_phmululCTE->Release();
	CRefCount::SafeRelease(m_pdrgpulOutputColRefs);
	CRefCount::SafeRelease(m_pdrgpmdname);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PdrgpulOutputColRefs
//
//	@doc:
// 		Return the array of query output column reference id
//
//---------------------------------------------------------------------------
ULongPtrArray *
CTranslatorDXLToExpr::PdrgpulOutputColRefs()
{
	GPOS_ASSERT(nullptr != m_pdrgpulOutputColRefs);
	return m_pdrgpulOutputColRefs;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::Pexpr
//
//	@doc:
//		Translate a DXL tree into an Expr Tree
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::Pexpr(const CDXLNode *dxlnode,
							const CDXLNodeArray *query_output_dxlnode_array,
							const CDXLNodeArray *cte_producers)
{
	GPOS_ASSERT(nullptr == m_pdrgpulOutputColRefs);
	GPOS_ASSERT(nullptr == m_phmulpdxlnCTEProducer);
	GPOS_ASSERT(nullptr != dxlnode && nullptr != dxlnode->GetOperator());
	GPOS_ASSERT(nullptr != query_output_dxlnode_array);

	m_phmulpdxlnCTEProducer = GPOS_NEW(m_mp) IdToCDXLNodeMap(m_mp);
	const ULONG ulCTEs = cte_producers->Size();
	for (ULONG ul = 0; ul < ulCTEs; ul++)
	{
		CDXLNode *pdxlnCTE = (*cte_producers)[ul];
		CDXLLogicalCTEProducer *pdxlopCTEProducer =
			CDXLLogicalCTEProducer::Cast(pdxlnCTE->GetOperator());

		pdxlnCTE->AddRef();
		BOOL fres GPOS_ASSERTS_ONLY = m_phmulpdxlnCTEProducer->Insert(
			GPOS_NEW(m_mp) ULONG(pdxlopCTEProducer->Id()), pdxlnCTE);
		GPOS_ASSERT(fres);
	}

	// translate main DXL tree
	CExpression *pexpr = Pexpr(dxlnode);
	GPOS_ASSERT(nullptr != pexpr);

	m_phmulpdxlnCTEProducer->Release();
	m_phmulpdxlnCTEProducer = nullptr;

	// generate the array of output column reference ids and column names
	m_pdrgpulOutputColRefs = GPOS_NEW(m_mp) ULongPtrArray(m_mp);
	m_pdrgpmdname = GPOS_NEW(m_mp) CMDNameArray(m_mp);

	BOOL fGenerateRequiredColumns =
		COperator::EopLogicalUpdate != pexpr->Pop()->Eopid();

	const ULONG length = query_output_dxlnode_array->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CDXLNode *pdxlnIdent = (*query_output_dxlnode_array)[ul];

		// get dxl scalar identifier
		CDXLScalarIdent *pdxlopIdent =
			CDXLScalarIdent::Cast(pdxlnIdent->GetOperator());

		// get the dxl column reference
		const CDXLColRef *dxl_colref = pdxlopIdent->GetDXLColRef();
		GPOS_ASSERT(nullptr != dxl_colref);
		const ULONG colid = dxl_colref->Id();

		// get its column reference from the hash map
		const CColRef *colref = LookupColRef(m_phmulcr, colid);

		if (fGenerateRequiredColumns)
		{
			const ULONG ulColRefId = colref->Id();
			ULONG *pulCopy = GPOS_NEW(m_mp) ULONG(ulColRefId);
			// add to the array of output column reference ids
			m_pdrgpulOutputColRefs->Append(pulCopy);

			// get the column names and add it to the array of output column names
			CMDName *mdname =
				GPOS_NEW(m_mp) CMDName(m_mp, dxl_colref->MdName()->GetMDName());
			m_pdrgpmdname->Append(mdname);
		}
	}

	return pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::Pexpr
//
//	@doc:
//		Translates a DXL tree into a Expr Tree
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::Pexpr(const CDXLNode *dxlnode)
{
	// recursive function - check stack
	GPOS_CHECK_STACK_SIZE;
	GPOS_ASSERT(nullptr != dxlnode && nullptr != dxlnode->GetOperator());

	CDXLOperator *dxl_op = dxlnode->GetOperator();

	CExpression *pexpr = nullptr;
	switch (dxl_op->GetDXLOperatorType())
	{
		case EdxloptypeLogical:
			pexpr = PexprLogical(dxlnode);
			break;

		case EdxloptypeScalar:
			pexpr = PexprScalar(dxlnode);
			break;

		default:
			GPOS_RAISE(gpopt::ExmaGPOPT, gpopt::ExmiUnsupportedOp,
					   dxlnode->GetOperator()->GetOpNameStr()->GetBuffer());
	}

	return pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprTranslateQuery
//
//	@doc:
// 		 Main driver for translating dxl query with its associated output
//		columns and CTEs
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprTranslateQuery(
	const CDXLNode *dxlnode, const CDXLNodeArray *query_output_dxlnode_array,
	const CDXLNodeArray *cte_producers)
{
	CAutoTimer at("\n[OPT]: DXL To Expr Translation Time",
				  GPOS_FTRACE(EopttracePrintOptimizationStatistics));

	CExpression *pexpr =
		Pexpr(dxlnode, query_output_dxlnode_array, cte_producers);

	// We need to mark all the colrefs which are not being referenced in the query as unused.
	// This needs to be done here after translating since we won't know which columns are
	// required until entire DXL tree has been processed
	MarkUnknownColsAsUnused();

	return pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprTranslateScalar
//
//	@doc:
// 		 Translate a dxl scalar expression
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprTranslateScalar(const CDXLNode *dxlnode,
										   CColRefArray *colref_array,
										   ULongPtrArray *pdrgpul)
{
	GPOS_ASSERT_IMP(nullptr != pdrgpul, nullptr != colref_array);
	GPOS_ASSERT_IMP(nullptr != pdrgpul,
					pdrgpul->Size() == colref_array->Size());

	if (EdxloptypeScalar != dxlnode->GetOperator()->GetDXLOperatorType())
	{
		GPOS_RAISE(gpopt::ExmaGPOPT, gpopt::ExmiUnexpectedOp,
				   dxlnode->GetOperator()->GetOpNameStr()->GetBuffer());
	}

	if (nullptr != colref_array)
	{
		const ULONG length = colref_array->Size();
		for (ULONG ul = 0; ul < length; ul++)
		{
			CColRef *colref = (*colref_array)[ul];
			// copy key
			ULONG *pulKey = nullptr;
			if (nullptr == pdrgpul)
			{
				pulKey = GPOS_NEW(m_mp) ULONG(ul + 1);
			}
			else
			{
				pulKey = GPOS_NEW(m_mp) ULONG(*((*pdrgpul)[ul]) + 1);
			}
#ifdef GPOS_DEBUG
			BOOL fres =
#endif	// GPOS_DEBUG
				m_phmulcr->Insert(pulKey, colref);
			GPOS_ASSERT(fres);
		}
	}

	return PexprScalar(dxlnode);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprLogical
//
//	@doc:
//		Translates a DXL Logical Op into a Expr
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprLogical(const CDXLNode *dxlnode)
{
	// recursive function - check stack
	GPOS_CHECK_STACK_SIZE;

	GPOS_ASSERT(nullptr != dxlnode);
	GPOS_ASSERT(EdxloptypeLogical ==
				dxlnode->GetOperator()->GetDXLOperatorType());
	CDXLOperator *dxl_op = dxlnode->GetOperator();

	switch (dxl_op->GetDXLOperator())
	{
		case EdxlopLogicalGet:
		case EdxlopLogicalForeignGet:
			return CTranslatorDXLToExpr::PexprLogicalGet(dxlnode);
		case EdxlopLogicalTVF:
			return CTranslatorDXLToExpr::PexprLogicalTVF(dxlnode);
		case EdxlopLogicalSelect:
			return CTranslatorDXLToExpr::PexprLogicalSelect(dxlnode);
		case EdxlopLogicalProject:
			return CTranslatorDXLToExpr::PexprLogicalProject(dxlnode);
		case EdxlopLogicalCTEAnchor:
			return CTranslatorDXLToExpr::PexprLogicalCTEAnchor(dxlnode);
		case EdxlopLogicalCTEProducer:
			return CTranslatorDXLToExpr::PexprLogicalCTEProducer(dxlnode);
		case EdxlopLogicalCTEConsumer:
			return CTranslatorDXLToExpr::PexprLogicalCTEConsumer(dxlnode);
		case EdxlopLogicalGrpBy:
			return CTranslatorDXLToExpr::PexprLogicalGroupBy(dxlnode);
		case EdxlopLogicalLimit:
			return CTranslatorDXLToExpr::PexprLogicalLimit(dxlnode);
		case EdxlopLogicalJoin:
			return CTranslatorDXLToExpr::PexprLogicalJoin(dxlnode);
		case EdxlopLogicalConstTable:
			return CTranslatorDXLToExpr::PexprLogicalConstTableGet(dxlnode);
		case EdxlopLogicalSetOp:
			return CTranslatorDXLToExpr::PexprLogicalSetOp(dxlnode);
		case EdxlopLogicalWindow:
			return CTranslatorDXLToExpr::PexprLogicalSeqPr(dxlnode);
		case EdxlopLogicalInsert:
			return CTranslatorDXLToExpr::PexprLogicalInsert(dxlnode);
		case EdxlopLogicalDelete:
			return CTranslatorDXLToExpr::PexprLogicalDelete(dxlnode);
		case EdxlopLogicalUpdate:
			return CTranslatorDXLToExpr::PexprLogicalUpdate(dxlnode);
		case EdxlopLogicalCTAS:
			return CTranslatorDXLToExpr::PexprLogicalCTAS(dxlnode);
		default:
			GPOS_RAISE(gpopt::ExmaGPOPT, gpopt::ExmiUnsupportedOp,
					   dxl_op->GetOpNameStr()->GetBuffer());
			return nullptr;
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprLogicalTVF
//
//	@doc:
// 		Create a logical TVF expression from its DXL representation
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprLogicalTVF(const CDXLNode *dxlnode)
{
	CDXLLogicalTVF *dxl_op = CDXLLogicalTVF::Cast(dxlnode->GetOperator());
	GPOS_ASSERT(nullptr != dxl_op->MdName()->GetMDName());

	// populate column information
	const ULONG ulColumns = dxl_op->Arity();
	GPOS_ASSERT(0 < ulColumns);

	CColumnDescriptorArray *pdrgpcoldesc =
		GPOS_NEW(m_mp) CColumnDescriptorArray(m_mp);

	for (ULONG ul = 0; ul < ulColumns; ul++)
	{
		const CDXLColDescr *pdxlcoldesc = dxl_op->GetColumnDescrAt(ul);
		GPOS_ASSERT(pdxlcoldesc->MdidType()->IsValid());

		const IMDType *pmdtype = m_pmda->RetrieveType(pdxlcoldesc->MdidType());

		GPOS_ASSERT(nullptr != pdxlcoldesc->MdName()->GetMDName()->GetBuffer());
		CWStringConst strColName(
			m_mp, pdxlcoldesc->MdName()->GetMDName()->GetBuffer());

		INT attrnum = pdxlcoldesc->AttrNum();
		CColumnDescriptor *pcoldesc = GPOS_NEW(m_mp)
			CColumnDescriptor(m_mp, pmdtype, pdxlcoldesc->TypeModifier(),
							  CName(m_mp, &strColName), attrnum,
							  true,	 // is_nullable
							  pdxlcoldesc->Width());
		pdrgpcoldesc->Append(pcoldesc);
	}

	// create a logical TVF operator
	IMDId *mdid_func = dxl_op->FuncMdId();
	mdid_func->AddRef();

	IMDId *mdid_return_type = dxl_op->ReturnTypeMdId();
	mdid_return_type->AddRef();
	CLogicalTVF *popTVF = GPOS_NEW(m_mp) CLogicalTVF(
		m_mp, mdid_func, mdid_return_type,
		GPOS_NEW(m_mp)
			CWStringConst(m_mp, dxl_op->MdName()->GetMDName()->GetBuffer()),
		pdrgpcoldesc);

	// create expression containing the logical TVF operator
	CExpression *pexpr = nullptr;
	const ULONG arity = dxlnode->Arity();
	if (0 < arity)
	{
		// translate function arguments
		CExpressionArray *pdrgpexprArgs = PdrgpexprChildren(dxlnode);

		pexpr = GPOS_NEW(m_mp) CExpression(m_mp, popTVF, pdrgpexprArgs);
	}
	else
	{
		// function has no arguments
		pexpr = GPOS_NEW(m_mp) CExpression(m_mp, popTVF);
	}

	// construct the mapping between the DXL ColId and CColRef
	ConstructDXLColId2ColRefMapping(dxl_op->GetDXLColumnDescrArray(),
									popTVF->PdrgpcrOutput());

	if (!popTVF->FuncMdId()->IsValid())
	{
		return pexpr;
	}

	const IMDFunction *pmdfunc = m_pmda->RetrieveFunc(mdid_func);

	if (IMDFunction::EfsVolatile == pmdfunc->GetFuncStability())
	{
		COptCtxt::PoctxtFromTLS()->SetHasVolatileFunc();
	}

	return pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprLogicalGet
//
//	@doc:
// 		Create a Expr logical get from a DXL logical get
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprLogicalGet(const CDXLNode *dxlnode)
{
	CDXLOperator *dxl_op = dxlnode->GetOperator();
	Edxlopid edxlopid = dxl_op->GetDXLOperator();

	// translate the table descriptor
	CDXLTableDescr *table_descr =
		CDXLLogicalGet::Cast(dxl_op)->GetDXLTableDescr();

	GPOS_ASSERT(nullptr != table_descr);
	GPOS_ASSERT(nullptr != table_descr->MdName()->GetMDName());

	CTableDescriptor *ptabdesc = Ptabdesc(table_descr);

	CWStringConst strAlias(m_mp,
						   table_descr->MdName()->GetMDName()->GetBuffer());

	// create a logical get or dynamic get operator
	CName *pname = GPOS_NEW(m_mp) CName(m_mp, CName(&strAlias));
	CLogical *popGet = nullptr;
	CColRefArray *colref_array = nullptr;

	const IMDRelation *pmdrel = m_pmda->RetrieveRel(table_descr->MDId());
	if (pmdrel->IsPartitioned())
	{
		GPOS_ASSERT(EdxlopLogicalGet == edxlopid ||
					EdxlopLogicalForeignGet == edxlopid);

		IMdIdArray *partition_mdids = pmdrel->ChildPartitionMdids();
		IMdIdArray *foreign_server_mdids = GPOS_NEW(m_mp) IMdIdArray(m_mp);
		for (ULONG ul = 0; ul < partition_mdids->Size(); ++ul)
		{
			IMDId *part_mdid = (*partition_mdids)[ul];
			const IMDRelation *partrel = m_pmda->RetrieveRel(part_mdid);

			if (partrel->IsPartitioned())
			{
				// Multi-level partitioned tables are unsupported - fall back
				GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported,
						   GPOS_WSZ_LIT("Multi-level partitioned tables"));
			}

			// store array of foreign partitions
			IMDId *foreign_server_mdid = nullptr;
			if (IMDRelation::ErelstorageForeign ==
				partrel->RetrieveRelStorageType())
			{
				foreign_server_mdid = partrel->ForeignServer();
				foreign_server_mdid->AddRef();
			}
			else
			{
				// not foreign, store as invalid mdid
				foreign_server_mdid =
					GPOS_NEW(m_mp) CMDIdGPDB(CMDIdGPDB::m_mdid_invalid_key);
			}
			foreign_server_mdids->Append(foreign_server_mdid);
		}

		// generate a part index id
		ULONG part_idx_id = COptCtxt::PoctxtFromTLS()->UlPartIndexNextVal();
		partition_mdids->AddRef();
		popGet = GPOS_NEW(m_mp)
			CLogicalDynamicGet(m_mp, pname, ptabdesc, part_idx_id,
							   partition_mdids, foreign_server_mdids);
		CLogicalDynamicGet *popDynamicGet =
			CLogicalDynamicGet::PopConvert(popGet);

		// get the output column references from the dynamic get
		colref_array = popDynamicGet->PdrgpcrOutput();
	}
	else
	{
		if (EdxlopLogicalGet == edxlopid)
		{
			popGet = GPOS_NEW(m_mp) CLogicalGet(m_mp, pname, ptabdesc);
		}
		else
		{
			GPOS_ASSERT(EdxlopLogicalForeignGet == edxlopid);
			popGet = GPOS_NEW(m_mp) CLogicalForeignGet(m_mp, pname, ptabdesc);
		}

		// get the output column references
		colref_array = CLogicalGet::PopConvert(popGet)->PdrgpcrOutput();
	}

	CExpression *pexpr = GPOS_NEW(m_mp) CExpression(m_mp, popGet);

	GPOS_ASSERT(nullptr != colref_array);
	GPOS_ASSERT(colref_array->Size() == table_descr->Arity());

	const ULONG ulColumns = colref_array->Size();
	// construct the mapping between the DXL ColId and CColRef
	for (ULONG ul = 0; ul < ulColumns; ul++)
	{
		CColRef *colref = (*colref_array)[ul];
		const CDXLColDescr *pdxlcd = table_descr->GetColumnDescrAt(ul);
		GPOS_ASSERT(nullptr != colref);
		GPOS_ASSERT(nullptr != pdxlcd && !pdxlcd->IsDropped());

		// copy key
		ULONG *pulKey = GPOS_NEW(m_mp) ULONG(pdxlcd->Id());
		BOOL fres = m_phmulcr->Insert(pulKey, colref);
		colref->SetMdidTable(ptabdesc->MDId());

		if (!fres)
		{
			GPOS_DELETE(pulKey);
		}
	}

	return pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprLogicalSetOp
//
//	@doc:
// 		Create a logical set operator from a DXL set operator
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprLogicalSetOp(const CDXLNode *dxlnode)
{
	GPOS_ASSERT(nullptr != dxlnode);

	CDXLLogicalSetOp *dxl_op = CDXLLogicalSetOp::Cast(dxlnode->GetOperator());

#ifdef GPOS_DEBUG
	const ULONG arity = dxlnode->Arity();
#endif	// GPOS_DEBUG

	GPOS_ASSERT(2 <= arity);
	GPOS_ASSERT(arity == dxl_op->ChildCount());

	// array of input column reference
	CColRef2dArray *pdrgdrgpcrInput = GPOS_NEW(m_mp) CColRef2dArray(m_mp);
	// array of output column descriptors
	ULongPtrArray *pdrgpulOutput = GPOS_NEW(m_mp) ULongPtrArray(m_mp);

	CExpressionArray *pdrgpexpr =
		PdrgpexprPreprocessSetOpInputs(dxlnode, pdrgdrgpcrInput, pdrgpulOutput);

	// create an array of output column references
	CColRefArray *pdrgpcrOutput = CTranslatorDXLToExprUtils::Pdrgpcr(
		m_mp, m_phmulcr, pdrgpulOutput /*array of colids of the first child*/);

	pdrgpulOutput->Release();

	CLogicalSetOp *pop = nullptr;
	switch (dxl_op->GetSetOpType())
	{
		case EdxlsetopUnion:
		{
			pop = GPOS_NEW(m_mp)
				CLogicalUnion(m_mp, pdrgpcrOutput, pdrgdrgpcrInput);
			break;
		}

		case EdxlsetopUnionAll:
		{
			pop = GPOS_NEW(m_mp)
				CLogicalUnionAll(m_mp, pdrgpcrOutput, pdrgdrgpcrInput);
			break;
		}

		case EdxlsetopDifference:
		{
			pop = GPOS_NEW(m_mp)
				CLogicalDifference(m_mp, pdrgpcrOutput, pdrgdrgpcrInput);
			break;
		}

		case EdxlsetopIntersect:
		{
			pop = GPOS_NEW(m_mp)
				CLogicalIntersect(m_mp, pdrgpcrOutput, pdrgdrgpcrInput);
			break;
		}

		case EdxlsetopDifferenceAll:
		{
			pop = GPOS_NEW(m_mp)
				CLogicalDifferenceAll(m_mp, pdrgpcrOutput, pdrgdrgpcrInput);
			break;
		}

		case EdxlsetopIntersectAll:
		{
			pop = GPOS_NEW(m_mp)
				CLogicalIntersectAll(m_mp, pdrgpcrOutput, pdrgdrgpcrInput);
			break;
		}

		default:
			GPOS_RAISE(gpopt::ExmaGPOPT, gpopt::ExmiUnsupportedOp,
					   dxl_op->GetOpNameStr()->GetBuffer());
	}

	GPOS_ASSERT(nullptr != pop);

	return GPOS_NEW(m_mp) CExpression(m_mp, pop, pdrgpexpr);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprCastPrjElem
//
//	@doc:
//		Return a project element on a cast expression
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprCastPrjElem(IMDId *pmdidSource, IMDId *mdid_dest,
									   const CColRef *pcrToCast,
									   CColRef *pcrToReturn)
{
	const IMDCast *pmdcast = m_pmda->Pmdcast(pmdidSource, mdid_dest);
	mdid_dest->AddRef();
	pmdcast->GetCastFuncMdId()->AddRef();
	CExpression *pexprCast;

	if (pmdcast->GetMDPathType() == IMDCast::EmdtArrayCoerce)
	{
		CMDArrayCoerceCastGPDB *parrayCoerceCast =
			(CMDArrayCoerceCastGPDB *) pmdcast;
		pexprCast = GPOS_NEW(m_mp) CExpression(
			m_mp,
			GPOS_NEW(m_mp) CScalarArrayCoerceExpr(
				m_mp, parrayCoerceCast->GetCastFuncMdId(), mdid_dest,
				parrayCoerceCast->TypeModifier(),
				parrayCoerceCast->IsExplicit(),
				(COperator::ECoercionForm) parrayCoerceCast->GetCoercionForm(),
				parrayCoerceCast->Location()),
			GPOS_NEW(m_mp) CExpression(
				m_mp, GPOS_NEW(m_mp) CScalarIdent(m_mp, pcrToCast)));
	}
	else
	{
		pexprCast = GPOS_NEW(m_mp) CExpression(
			m_mp,
			GPOS_NEW(m_mp)
				CScalarCast(m_mp, mdid_dest, pmdcast->GetCastFuncMdId(),
							pmdcast->IsBinaryCoercible()),
			GPOS_NEW(m_mp) CExpression(
				m_mp, GPOS_NEW(m_mp) CScalarIdent(m_mp, pcrToCast)));
	}

	return GPOS_NEW(m_mp) CExpression(
		m_mp, GPOS_NEW(m_mp) CScalarProjectElement(m_mp, pcrToReturn),
		pexprCast);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::BuildSetOpChild
//
//	@doc:
//		Build expression and input columns of SetOp Child
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToExpr::BuildSetOpChild(
	const CDXLNode *pdxlnSetOp, ULONG child_index,
	CExpression **ppexprChild,	   // output: generated child expression
	CColRefArray **ppdrgpcrChild,  // output: generated child input columns
	CExpressionArray **
		ppdrgpexprChildProjElems  // output: project elements to remap child input columns
)
{
	GPOS_ASSERT(nullptr != pdxlnSetOp);
	GPOS_ASSERT(nullptr != ppexprChild);
	GPOS_ASSERT(nullptr != ppdrgpcrChild);
	GPOS_ASSERT(nullptr != ppdrgpexprChildProjElems);
	GPOS_ASSERT(nullptr == *ppdrgpexprChildProjElems);

	const CDXLLogicalSetOp *dxl_op =
		CDXLLogicalSetOp::Cast(pdxlnSetOp->GetOperator());
	const CDXLNode *child_dxlnode = (*pdxlnSetOp)[child_index];

	// array of project elements to remap child input columns
	*ppdrgpexprChildProjElems = GPOS_NEW(m_mp) CExpressionArray(m_mp);

	// array of child input column
	*ppdrgpcrChild = GPOS_NEW(m_mp) CColRefArray(m_mp);

	// translate child
	*ppexprChild = PexprLogical(child_dxlnode);

	const ULongPtrArray *pdrgpulInput =
		dxl_op->GetInputColIdArrayAt(child_index);
	const ULONG ulInputCols = pdrgpulInput->Size();
	CColRefSet *pcrsChildOutput = (*ppexprChild)->DeriveOutputColumns();
	for (ULONG ulColPos = 0; ulColPos < ulInputCols; ulColPos++)
	{
		// column identifier of the input column
		ULONG colid = *(*pdrgpulInput)[ulColPos];
		const CColRef *colref = LookupColRef(m_phmulcr, colid);

		// corresponding output column descriptor
		const CDXLColDescr *pdxlcdOutput = dxl_op->GetColumnDescrAt(ulColPos);

		// check if a cast function needs to be introduced
		IMDId *pmdidSource = colref->RetrieveType()->MDId();
		IMDId *mdid_dest = pdxlcdOutput->MdidType();

		if (FCastingUnknownType(pmdidSource, mdid_dest))
		{
			GPOS_RAISE(gpopt::ExmaGPOPT, gpopt::ExmiUnsupportedOp,
					   GPOS_WSZ_LIT("Casting of columns of unknown data type"));
		}

		const IMDType *pmdtype = m_pmda->RetrieveType(mdid_dest);
		INT type_modifier = pdxlcdOutput->TypeModifier();

		BOOL fEqualTypes = IMDId::MDIdCompare(pmdidSource, mdid_dest);
		BOOL fFirstChild = (0 == child_index);

		if (!pcrsChildOutput->FMember(colref))
		{
			// input column is an outer reference, add a project element for input column

			// add the colref to the hash map between DXL ColId and colref as they can used above the setop
			CColRef *new_colref = PcrCreate(colref, pmdtype, type_modifier,
											fFirstChild, pdxlcdOutput->Id());
			(*ppdrgpcrChild)->Append(new_colref);

			CExpression *pexprChildProjElem = nullptr;
			if (fEqualTypes)
			{
				// project child input column
				pexprChildProjElem = GPOS_NEW(m_mp) CExpression(
					m_mp,
					GPOS_NEW(m_mp) CScalarProjectElement(m_mp, new_colref),
					GPOS_NEW(m_mp) CExpression(
						m_mp, GPOS_NEW(m_mp) CScalarIdent(m_mp, colref)));
			}
			else
			{
				// introduce cast expression
				pexprChildProjElem = PexprCastPrjElem(pmdidSource, mdid_dest,
													  colref, new_colref);
			}

			(*ppdrgpexprChildProjElems)->Append(pexprChildProjElem);
			continue;
		}

		if (fEqualTypes)
		{
			// no cast function needed, add the colref to the array of input colrefs
			(*ppdrgpcrChild)->Append(const_cast<CColRef *>(colref));
		}
		else
		{
			// add the colref to the hash map between DXL ColId and colref as they can used above the setop
			CColRef *new_colref = PcrCreate(colref, pmdtype, type_modifier,
											fFirstChild, pdxlcdOutput->Id());
			(*ppdrgpcrChild)->Append(new_colref);

			// introduce cast expression for input column
			CExpression *pexprChildProjElem =
				PexprCastPrjElem(pmdidSource, mdid_dest, colref, new_colref);
			(*ppdrgpexprChildProjElems)->Append(pexprChildProjElem);
		}
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PdrgpexprPreprocessSetOpInputs
//
//	@doc:
//		Pre-process inputs to the set operator and add casting when needed
//
//---------------------------------------------------------------------------
CExpressionArray *
CTranslatorDXLToExpr::PdrgpexprPreprocessSetOpInputs(
	const CDXLNode *dxlnode, CColRef2dArray *pdrgdrgpcrInput,
	ULongPtrArray *pdrgpulOutput)
{
	GPOS_ASSERT(nullptr != dxlnode);
	GPOS_ASSERT(nullptr != pdrgdrgpcrInput);
	GPOS_ASSERT(nullptr != pdrgpulOutput);

	// array of child expression
	CExpressionArray *pdrgpexpr = GPOS_NEW(m_mp) CExpressionArray(m_mp);

	CDXLLogicalSetOp *dxl_op = CDXLLogicalSetOp::Cast(dxlnode->GetOperator());

	const ULONG arity = dxlnode->Arity();
	GPOS_ASSERT(2 <= arity);
	GPOS_ASSERT(arity == dxl_op->ChildCount());

	const ULONG ulOutputCols = dxl_op->Arity();

	for (ULONG ul = 0; ul < arity; ul++)
	{
		CExpression *pexprChild = nullptr;
		CColRefArray *pdrgpcrInput = nullptr;
		CExpressionArray *pdrgpexprChildProjElems = nullptr;
		BuildSetOpChild(dxlnode, ul, &pexprChild, &pdrgpcrInput,
						&pdrgpexprChildProjElems);
		GPOS_ASSERT(ulOutputCols == pdrgpcrInput->Size());
		GPOS_ASSERT(nullptr != pexprChild);

		pdrgdrgpcrInput->Append(pdrgpcrInput);

		if (0 < pdrgpexprChildProjElems->Size())
		{
			CExpression *pexprChildProject = GPOS_NEW(m_mp) CExpression(
				m_mp, GPOS_NEW(m_mp) CLogicalProject(m_mp), pexprChild,
				GPOS_NEW(m_mp)
					CExpression(m_mp, GPOS_NEW(m_mp) CScalarProjectList(m_mp),
								pdrgpexprChildProjElems));
			pdrgpexpr->Append(pexprChildProject);
		}
		else
		{
			pdrgpexpr->Append(pexprChild);
			pdrgpexprChildProjElems->Release();
		}
	}

	// create the set operation's array of output column identifiers
	for (ULONG ulOutputColPos = 0; ulOutputColPos < ulOutputCols;
		 ulOutputColPos++)
	{
		const CDXLColDescr *pdxlcdOutput =
			dxl_op->GetColumnDescrAt(ulOutputColPos);
		pdrgpulOutput->Append(GPOS_NEW(m_mp) ULONG(pdxlcdOutput->Id()));
	}

	return pdrgpexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::FCastingUnknownType
//
//	@doc:
//		Check if we currently support the casting of such column types
//---------------------------------------------------------------------------
BOOL
CTranslatorDXLToExpr::FCastingUnknownType(IMDId *pmdidSource, IMDId *mdid_dest)
{
	return ((pmdidSource->Equals(&CMDIdGPDB::m_mdid_unknown) ||
			 mdid_dest->Equals(&CMDIdGPDB::m_mdid_unknown)));
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::LookupColRef
//
//	@doc:
// 		Look up the column reference in the hash map. We raise an exception if
//		the column is not found
//---------------------------------------------------------------------------
CColRef *
CTranslatorDXLToExpr::LookupColRef(UlongToColRefMap *colref_mapping,
								   ULONG colid)
{
	GPOS_ASSERT(nullptr != colref_mapping);
	GPOS_ASSERT(gpos::ulong_max != colid);

	// get its column reference from the hash map
	CColRef *colref = colref_mapping->Find(&colid);
	if (nullptr == colref)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXL2ExprAttributeNotFound, colid);
	}

	colref->MarkAsUsed();
	return colref;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PcrCreate
//
//	@doc:
// 		Create new column reference and add to the hashmap maintaining
//		the mapping between DXL ColIds and column reference.
//
//---------------------------------------------------------------------------
CColRef *
CTranslatorDXLToExpr::PcrCreate(const CColRef *colref, const IMDType *pmdtype,
								INT type_modifier, BOOL fStoreMapping,
								ULONG colid)
{
	// generate a new column reference
	CName name(colref->Name().Pstr());
	CColRef *new_colref = m_pcf->PcrCreate(pmdtype, type_modifier, name);

	if (fStoreMapping)
	{
		BOOL result GPOS_ASSERTS_ONLY =
			m_phmulcr->Insert(GPOS_NEW(m_mp) ULONG(colid), new_colref);

		GPOS_ASSERT(result);
	}

	return new_colref;
}
//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::pdrgpcrOutput
//
//	@doc:
// 		Construct an array of new column references from the array of
//		DXL column descriptors
//
//---------------------------------------------------------------------------
CColRefArray *
CTranslatorDXLToExpr::Pdrgpcr(const CDXLColDescrArray *dxl_col_descr_array)
{
	GPOS_ASSERT(nullptr != dxl_col_descr_array);
	CColRefArray *pdrgpcrOutput = GPOS_NEW(m_mp) CColRefArray(m_mp);
	ULONG ulOutputCols = dxl_col_descr_array->Size();
	for (ULONG ul = 0; ul < ulOutputCols; ul++)
	{
		CDXLColDescr *pdxlcd = (*dxl_col_descr_array)[ul];
		IMDId *mdid = pdxlcd->MdidType();
		const IMDType *pmdtype = m_pmda->RetrieveType(mdid);

		CName name(pdxlcd->MdName()->GetMDName());
		// generate a new column reference
		CColRef *colref =
			m_pcf->PcrCreate(pmdtype, pdxlcd->TypeModifier(), name);
		pdrgpcrOutput->Append(colref);
	}

	return pdrgpcrOutput;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::ConstructDXLColId2ColRefMapping
//
//	@doc:
// 		Construct the mapping between the DXL ColId and CColRef
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToExpr::ConstructDXLColId2ColRefMapping(
	const CDXLColDescrArray *dxl_col_descr_array,
	const CColRefArray *colref_array)
{
	GPOS_ASSERT(nullptr != dxl_col_descr_array);
	GPOS_ASSERT(nullptr != colref_array);

	const ULONG ulColumns = dxl_col_descr_array->Size();
	GPOS_ASSERT(colref_array->Size() == ulColumns);

	// construct the mapping between the DXL ColId and CColRef
	for (ULONG ul = 0; ul < ulColumns; ul++)
	{
		CColRef *colref = (*colref_array)[ul];
		GPOS_ASSERT(nullptr != colref);

		const CDXLColDescr *pdxlcd = (*dxl_col_descr_array)[ul];
		GPOS_ASSERT(nullptr != pdxlcd && !pdxlcd->IsDropped());

		// copy key
		ULONG *pulKey = GPOS_NEW(m_mp) ULONG(pdxlcd->Id());
		BOOL result GPOS_ASSERTS_ONLY = m_phmulcr->Insert(pulKey, colref);

		GPOS_ASSERT(result);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprLogicalSelect
//
//	@doc:
// 		Create a logical select expr from a DXL logical select
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprLogicalSelect(const CDXLNode *dxlnode)
{
	GPOS_ASSERT(nullptr != dxlnode);

	// translate the child dxl node
	CDXLNode *child_dxlnode = (*dxlnode)[1];
	CExpression *pexprChild = PexprLogical(child_dxlnode);

	// translate scalar condition
	CDXLNode *pdxlnCond = (*dxlnode)[0];
	CExpression *pexprCond = PexprScalar(pdxlnCond);
	CLogicalSelect *plgselect = GPOS_NEW(m_mp) CLogicalSelect(m_mp);
	CExpression *pexprSelect =
		GPOS_NEW(m_mp) CExpression(m_mp, plgselect, pexprChild, pexprCond);

	return pexprSelect;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprLogicalProject
//
//	@doc:
// 		Create a logical project expr from a DXL logical project
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprLogicalProject(const CDXLNode *dxlnode)
{
	GPOS_ASSERT(nullptr != dxlnode);

	// translate the child dxl node
	CDXLNode *child_dxlnode = (*dxlnode)[1];
	CExpression *pexprChild = PexprLogical(child_dxlnode);

	// translate the project list
	CDXLNode *pdxlnPrL = (*dxlnode)[0];
	GPOS_ASSERT(EdxlopScalarProjectList ==
				pdxlnPrL->GetOperator()->GetDXLOperator());
	CExpression *pexprProjList = PexprScalarProjList(pdxlnPrL);

	CLogicalProject *popProject = GPOS_NEW(m_mp) CLogicalProject(m_mp);
	CExpression *pexprProject =
		GPOS_NEW(m_mp) CExpression(m_mp, popProject, pexprChild, pexprProjList);

	return pexprProject;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprLogicalCTEAnchor
//
//	@doc:
// 		Create a logical CTE anchor expr from a DXL logical CTE anchor
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprLogicalCTEAnchor(const CDXLNode *dxlnode)
{
	GPOS_ASSERT(nullptr != dxlnode);
	CDXLLogicalCTEAnchor *pdxlopCTEAnchor =
		CDXLLogicalCTEAnchor::Cast(dxlnode->GetOperator());
	ULONG ulCTEId = pdxlopCTEAnchor->Id();

	CDXLNode *pdxlnCTEProducer = m_phmulpdxlnCTEProducer->Find(&ulCTEId);
	GPOS_ASSERT(nullptr != pdxlnCTEProducer);

	ULONG id = UlMapCTEId(ulCTEId);
	// mark that we are about to start processing this new CTE and keep track
	// of the previous one
	ULONG ulCTEPrevious = m_ulCTEId;
	m_ulCTEId = id;
	CExpression *pexprProducer = Pexpr(pdxlnCTEProducer);
	GPOS_ASSERT(nullptr != pexprProducer);
	m_ulCTEId = ulCTEPrevious;

	CColRefSet *pcrsProducerOuter = pexprProducer->DeriveOuterReferences();
	if (0 < pcrsProducerOuter->Size())
	{
		GPOS_RAISE(gpopt::ExmaGPOPT, gpopt::ExmiUnsupportedOp,
				   GPOS_WSZ_LIT("CTE with outer references"));
	}

	COptCtxt::PoctxtFromTLS()->Pcteinfo()->AddCTEProducer(pexprProducer);
	pexprProducer->Release();

	// translate the child dxl node
	CExpression *pexprChild = PexprLogical((*dxlnode)[0]);

	return GPOS_NEW(m_mp) CExpression(
		m_mp, GPOS_NEW(m_mp) CLogicalCTEAnchor(m_mp, id), pexprChild);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprLogicalCTEProducer
//
//	@doc:
// 		Create a logical CTE producer expr from a DXL logical CTE producer
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprLogicalCTEProducer(const CDXLNode *dxlnode)
{
	GPOS_ASSERT(nullptr != dxlnode);
	CDXLLogicalCTEProducer *pdxlopCTEProducer =
		CDXLLogicalCTEProducer::Cast(dxlnode->GetOperator());
	ULONG id = UlMapCTEId(pdxlopCTEProducer->Id());

	// translate the child dxl node
	CExpression *pexprChild = PexprLogical((*dxlnode)[0]);

	// a column of the cte producer's child may be used in CTE producer output multiple times;
	// CTE consumer maintains a hash map between the cte producer columns to cte consumer columns.
	// To avoid losing mapping information of duplicate producer columns, we introduce a relabel
	// node (project element) for each duplicate entry of the producer column.

	CExpressionArray *pdrgpexprPrEl = GPOS_NEW(m_mp) CExpressionArray(m_mp);
	CColRefSet *pcrsProducer = GPOS_NEW(m_mp) CColRefSet(m_mp);
	CColRefArray *colref_array = GPOS_NEW(m_mp) CColRefArray(m_mp);

	ULongPtrArray *pdrgpulCols = pdxlopCTEProducer->GetOutputColIdsArray();
	const ULONG length = pdrgpulCols->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		ULONG *pulColId = (*pdrgpulCols)[ul];
		CColRef *colref = LookupColRef(m_phmulcr, *pulColId);
		GPOS_ASSERT(nullptr != colref);

		if (pcrsProducer->FMember(colref))
		{
			// the column was previously used, so introduce a project node to relabel
			// the next use of the column reference
			CColRef *new_colref = m_pcf->PcrCreate(colref);
			CExpression *pexprPrEl = CUtils::PexprScalarProjectElement(
				m_mp, new_colref,
				GPOS_NEW(m_mp) CExpression(
					m_mp, GPOS_NEW(m_mp) CScalarIdent(m_mp, colref)));
			pdrgpexprPrEl->Append(pexprPrEl);
			colref = new_colref;
		}

		colref_array->Append(colref);
		pcrsProducer->Include(colref);
	}

	GPOS_ASSERT(length == colref_array->Size());

	if (0 < pdrgpexprPrEl->Size())
	{
		pdrgpexprPrEl->AddRef();
		CExpression *pexprPr = GPOS_NEW(m_mp) CExpression(
			m_mp, GPOS_NEW(m_mp) CLogicalProject(m_mp), pexprChild,
			GPOS_NEW(m_mp) CExpression(
				m_mp, GPOS_NEW(m_mp) CScalarProjectList(m_mp), pdrgpexprPrEl));
		pexprChild = pexprPr;
	}

	// clean up
	pdrgpexprPrEl->Release();
	pcrsProducer->Release();

	return GPOS_NEW(m_mp) CExpression(
		m_mp, GPOS_NEW(m_mp) CLogicalCTEProducer(m_mp, id, colref_array),
		pexprChild);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprLogicalCTEConsumer
//
//	@doc:
// 		Create a logical CTE consumer expr from a DXL logical CTE consumer
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprLogicalCTEConsumer(const CDXLNode *dxlnode)
{
	GPOS_ASSERT(nullptr != dxlnode);

	CDXLLogicalCTEConsumer *pdxlopCTEConsumer =
		CDXLLogicalCTEConsumer::Cast(dxlnode->GetOperator());
	ULONG id = UlMapCTEId(pdxlopCTEConsumer->Id());

	ULongPtrArray *pdrgpulCols = pdxlopCTEConsumer->GetOutputColIdsArray();

	// create new col refs
	CCTEInfo *pcteinfo = COptCtxt::PoctxtFromTLS()->Pcteinfo();
	CExpression *pexprProducer = pcteinfo->PexprCTEProducer(id);
	GPOS_ASSERT(nullptr != pexprProducer);

	CColRefArray *pdrgpcrProducer =
		CLogicalCTEProducer::PopConvert(pexprProducer->Pop())->Pdrgpcr();
	CColRefArray *pdrgpcrConsumer = CUtils::PdrgpcrCopy(m_mp, pdrgpcrProducer);

	// add new colrefs to mapping
	const ULONG num_cols = pdrgpcrConsumer->Size();
	GPOS_ASSERT(pdrgpulCols->Size() == num_cols);
	for (ULONG ul = 0; ul < num_cols; ul++)
	{
		ULONG *pulColId = GPOS_NEW(m_mp) ULONG(*(*pdrgpulCols)[ul]);
		CColRef *colref = (*pdrgpcrConsumer)[ul];

		BOOL result GPOS_ASSERTS_ONLY = m_phmulcr->Insert(pulColId, colref);
		GPOS_ASSERT(result);
	}

	pcteinfo->IncrementConsumers(id, m_ulCTEId);
	return GPOS_NEW(m_mp) CExpression(
		m_mp, GPOS_NEW(m_mp) CLogicalCTEConsumer(m_mp, id, pdrgpcrConsumer));
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::UlMapCTEId
//
//	@doc:
// 		Return an new CTE id based on the CTE id used in DXL, since we may
//		introduce new CTEs during translation that did not exist in DXL
//
//---------------------------------------------------------------------------
ULONG
CTranslatorDXLToExpr::UlMapCTEId(const ULONG ulIdOld)
{
	ULONG *pulNewId = m_phmululCTE->Find(&ulIdOld);
	if (nullptr == pulNewId)
	{
		pulNewId = GPOS_NEW(m_mp)
			ULONG(COptCtxt::PoctxtFromTLS()->Pcteinfo()->next_id());

		BOOL fInserted GPOS_ASSERTS_ONLY =
			m_phmululCTE->Insert(GPOS_NEW(m_mp) ULONG(ulIdOld), pulNewId);
		GPOS_ASSERT(fInserted);
	}

	return *pulNewId;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprLogicalInsert
//
//	@doc:
// 		Create a logical DML on top of a project from a DXL logical insert
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprLogicalInsert(const CDXLNode *dxlnode)
{
	GPOS_ASSERT(nullptr != dxlnode);
	CDXLLogicalInsert *pdxlopInsert =
		CDXLLogicalInsert::Cast(dxlnode->GetOperator());

	// translate the child dxl node
	CDXLNode *child_dxlnode = (*dxlnode)[0];
	CExpression *pexprChild = PexprLogical(child_dxlnode);

	CTableDescriptor *ptabdesc = Ptabdesc(pdxlopInsert->GetDXLTableDescr());

	ULongPtrArray *pdrgpulSourceCols = pdxlopInsert->GetSrcColIdsArray();
	CColRefArray *colref_array =
		CTranslatorDXLToExprUtils::Pdrgpcr(m_mp, m_phmulcr, pdrgpulSourceCols);

	return GPOS_NEW(m_mp) CExpression(
		m_mp, GPOS_NEW(m_mp) CLogicalInsert(m_mp, ptabdesc, colref_array),
		pexprChild);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprLogicalDelete
//
//	@doc:
// 		Create a logical DML on top of a project from a DXL logical delete
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprLogicalDelete(const CDXLNode *dxlnode)
{
	GPOS_ASSERT(nullptr != dxlnode);
	CDXLLogicalDelete *pdxlopDelete =
		CDXLLogicalDelete::Cast(dxlnode->GetOperator());

	// translate the child dxl node
	CDXLNode *child_dxlnode = (*dxlnode)[0];
	CExpression *pexprChild = PexprLogical(child_dxlnode);

	CTableDescriptor *ptabdesc = Ptabdesc(pdxlopDelete->GetDXLTableDescr());

	if (COptCtxt::PoctxtFromTLS()->HasReplicatedTables())
	{
		GPOS_RAISE(gpopt::ExmaGPOPT, gpopt::ExmiUnsupportedOp,
				   GPOS_WSZ_LIT("Delete on replicated tables"));
	}

	ULONG ctid_colid = pdxlopDelete->GetCtIdColId();
	ULONG segid_colid = pdxlopDelete->GetSegmentIdColId();

	CColRef *pcrCtid = LookupColRef(m_phmulcr, ctid_colid);
	CColRef *pcrSegmentId = LookupColRef(m_phmulcr, segid_colid);

	ULongPtrArray *pdrgpulCols = pdxlopDelete->GetDeletionColIdArray();
	CColRefArray *colref_array =
		CTranslatorDXLToExprUtils::Pdrgpcr(m_mp, m_phmulcr, pdrgpulCols);

	return GPOS_NEW(m_mp) CExpression(
		m_mp,
		GPOS_NEW(m_mp)
			CLogicalDelete(m_mp, ptabdesc, colref_array, pcrCtid, pcrSegmentId),
		pexprChild);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprLogicalUpdate
//
//	@doc:
// 		Create a logical DML on top of a split from a DXL logical update
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprLogicalUpdate(const CDXLNode *dxlnode)
{
	GPOS_ASSERT(nullptr != dxlnode);
	CDXLLogicalUpdate *pdxlopUpdate =
		CDXLLogicalUpdate::Cast(dxlnode->GetOperator());

	// translate the child dxl node
	CDXLNode *child_dxlnode = (*dxlnode)[0];
	CExpression *pexprChild = PexprLogical(child_dxlnode);

	CTableDescriptor *ptabdesc = Ptabdesc(pdxlopUpdate->GetDXLTableDescr());

	if (COptCtxt::PoctxtFromTLS()->HasReplicatedTables())
	{
		GPOS_RAISE(gpopt::ExmaGPOPT, gpopt::ExmiUnsupportedOp,
				   GPOS_WSZ_LIT("Update on replicated tables"));
	}

	const IMDRelation *pmdrel = m_pmda->RetrieveRel(ptabdesc->MDId());
	if (pmdrel->IsPartitioned())
	{
		GPOS_ASSERT(EdxlopLogicalUpdate == pdxlopUpdate->GetDXLOperator());

		IMdIdArray *partition_mdids = pmdrel->ChildPartitionMdids();
		for (ULONG ul = 0; ul < partition_mdids->Size(); ++ul)
		{
			IMDId *part_mdid = (*partition_mdids)[ul];
			const IMDRelation *partrel = m_pmda->RetrieveRel(part_mdid);

			if (partrel->IsPartitioned())
			{
				// Multi-level partitioned tables are unsupported - fall back
				GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported,
						   GPOS_WSZ_LIT("Multi-level partitioned tables"));
			}
		}
	}

	ULONG ctid_colid = pdxlopUpdate->GetCtIdColId();
	ULONG segid_colid = pdxlopUpdate->GetSegmentIdColId();

	CColRef *pcrCtid = LookupColRef(m_phmulcr, ctid_colid);
	CColRef *pcrSegmentId = LookupColRef(m_phmulcr, segid_colid);

	ULongPtrArray *pdrgpulInsertCols = pdxlopUpdate->GetInsertionColIdArray();
	CColRefArray *pdrgpcrInsert =
		CTranslatorDXLToExprUtils::Pdrgpcr(m_mp, m_phmulcr, pdrgpulInsertCols);

	ULongPtrArray *pdrgpulDeleteCols = pdxlopUpdate->GetDeletionColIdArray();
	CColRefArray *pdrgpcrDelete =
		CTranslatorDXLToExprUtils::Pdrgpcr(m_mp, m_phmulcr, pdrgpulDeleteCols);

	return GPOS_NEW(m_mp)
		CExpression(m_mp,
					GPOS_NEW(m_mp) CLogicalUpdate(m_mp, ptabdesc, pdrgpcrDelete,
												  pdrgpcrInsert, pcrCtid,
												  pcrSegmentId, true),
					pexprChild);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprLogicalCTAS
//
//	@doc:
// 		Create a logical Insert from a logical DXL CTAS operator
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprLogicalCTAS(const CDXLNode *dxlnode)
{
	GPOS_ASSERT(nullptr != dxlnode);
	CDXLLogicalCTAS *pdxlopCTAS = CDXLLogicalCTAS::Cast(dxlnode->GetOperator());

	// translate the child dxl node
	CDXLNode *child_dxlnode = (*dxlnode)[0];
	CExpression *pexprChild = PexprLogical(child_dxlnode);

	RegisterMDRelationCtas(pdxlopCTAS);
	CTableDescriptor *ptabdesc = PtabdescFromCTAS(pdxlopCTAS);

	ULongPtrArray *pdrgpulSourceCols = pdxlopCTAS->GetSrcColidsArray();
	CColRefArray *colref_array =
		CTranslatorDXLToExprUtils::Pdrgpcr(m_mp, m_phmulcr, pdrgpulSourceCols);

	return GPOS_NEW(m_mp) CExpression(
		m_mp, GPOS_NEW(m_mp) CLogicalInsert(m_mp, ptabdesc, colref_array),
		pexprChild);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprLogicalGroupBy
//
//	@doc:
// 		Create a logical group by expr from a DXL logical group by aggregate
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprLogicalGroupBy(const CDXLNode *dxlnode)
{
	// get children
	CDXLLogicalGroupBy *pdxlopGrpby =
		CDXLLogicalGroupBy::Cast(dxlnode->GetOperator());
	CDXLNode *pdxlnPrL = (*dxlnode)[0];
	CDXLNode *child_dxlnode = (*dxlnode)[1];

	// translate the child dxl node
	CExpression *pexprChild = PexprLogical(child_dxlnode);

	// translate proj list
	CExpression *pexprProjList = PexprScalarProjList(pdxlnPrL);

	// translate grouping columns
	CColRefArray *pdrgpcrGroupingCols = CTranslatorDXLToExprUtils::Pdrgpcr(
		m_mp, m_phmulcr, pdxlopGrpby->GetGroupingColidArray());

	if (0 != pexprProjList->Arity())
	{
		GPOS_ASSERT(CUtils::FHasGlobalAggFunc(pexprProjList));
	}

	return GPOS_NEW(m_mp) CExpression(
		m_mp,
		GPOS_NEW(m_mp) CLogicalGbAgg(m_mp, pdrgpcrGroupingCols,
									 COperator::EgbaggtypeGlobal /*egbaggtype*/
									 ),
		pexprChild, pexprProjList);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprLogicalLimit
//
//	@doc:
// 		Create a logical limit expr from a DXL logical limit node
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprLogicalLimit(const CDXLNode *dxlnode)
{
	GPOS_ASSERT(nullptr != dxlnode &&
				EdxlopLogicalLimit == dxlnode->GetOperator()->GetDXLOperator());

	// get children
	CDXLNode *sort_col_list_dxlnode =
		(*dxlnode)[EdxllogicallimitIndexSortColList];
	CDXLNode *pdxlnCount = (*dxlnode)[EdxllogicallimitIndexLimitCount];
	CDXLNode *pdxlnOffset = (*dxlnode)[EdxllogicallimitIndexLimitOffset];
	CDXLNode *child_dxlnode = (*dxlnode)[EdxllogicallimitIndexChildPlan];

	// translate count
	CExpression *pexprLimitCount = nullptr;
	BOOL fHasCount = false;
	if (1 == pdxlnCount->Arity())
	{
		// translate limit count
		pexprLimitCount = Pexpr((*pdxlnCount)[0]);
		COperator *popCount = pexprLimitCount->Pop();
		BOOL fConst = (COperator::EopScalarConst == popCount->Eopid());
		if (!fConst ||
			(fConst &&
			 !CScalarConst::PopConvert(popCount)->GetDatum()->IsNull()))
		{
			fHasCount = true;
		}
	}
	else
	{
		// no limit count is specified, manufacture a null count
		pexprLimitCount =
			CUtils::PexprScalarConstInt8(m_mp, 0 /*val*/, true /*is_null*/);
	}

	// translate offset
	CExpression *pexprLimitOffset = nullptr;

	if (1 == pdxlnOffset->Arity())
	{
		pexprLimitOffset = Pexpr((*pdxlnOffset)[0]);
	}
	else
	{
		// manufacture an OFFSET 0
		pexprLimitOffset = CUtils::PexprScalarConstInt8(m_mp, 0 /*val*/);
	}

	// translate limit child
	CExpression *pexprChild = PexprLogical(child_dxlnode);

	// translate sort col list
	COrderSpec *pos = Pos(sort_col_list_dxlnode);

	BOOL fNonRemovable = CDXLLogicalLimit::Cast(dxlnode->GetOperator())
							 ->IsTopLimitUnderDMLorCTAS();
	CLogicalLimit *popLimit = GPOS_NEW(m_mp)
		CLogicalLimit(m_mp, pos, true /*fGlobal*/, fHasCount, fNonRemovable);
	return GPOS_NEW(m_mp) CExpression(m_mp, popLimit, pexprChild,
									  pexprLimitOffset, pexprLimitCount);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprLogicalSeqPr
//
//	@doc:
// 		Create a logical sequence expr from a DXL logical window
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprLogicalSeqPr(const CDXLNode *dxlnode)
{
	GPOS_ASSERT(nullptr != dxlnode);

	CDXLLogicalWindow *pdxlopWindow =
		CDXLLogicalWindow::Cast(dxlnode->GetOperator());

	CDXLNode *pdxlnWindowChild = (*dxlnode)[1];
	CExpression *pexprWindowChild = PexprLogical(pdxlnWindowChild);

	// maintains the map between window specification position -> list of project elements
	// used to generate a cascade of window nodes
	UlongToExprArrayMap *phmulpdrgpexpr =
		GPOS_NEW(m_mp) UlongToExprArrayMap(m_mp);

	CDXLNode *pdxlnPrL = (*dxlnode)[0];
	GPOS_ASSERT(EdxlopScalarProjectList ==
				pdxlnPrL->GetOperator()->GetDXLOperator());
	const ULONG arity = pdxlnPrL->Arity();
	GPOS_ASSERT(0 < arity);

	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnProjElem = (*pdxlnPrL)[ul];

		GPOS_ASSERT(nullptr != pdxlnProjElem);
		GPOS_ASSERT(EdxlopScalarProjectElem ==
						pdxlnProjElem->GetOperator()->GetDXLOperator() &&
					1 == pdxlnProjElem->Arity());

		CDXLNode *pdxlnPrElChild = (*pdxlnProjElem)[0];
		// expect the project list to be normalized and expect to only find window functions and scalar identifiers
		GPOS_ASSERT(EdxlopScalarIdent ==
						pdxlnPrElChild->GetOperator()->GetDXLOperator() ||
					EdxlopScalarWindowRef ==
						pdxlnPrElChild->GetOperator()->GetDXLOperator());
		CDXLScalarProjElem *pdxlopPrEl =
			CDXLScalarProjElem::Cast(pdxlnProjElem->GetOperator());

		if (EdxlopScalarWindowRef ==
			pdxlnPrElChild->GetOperator()->GetDXLOperator())
		{
			// translate window function
			CDXLScalarWindowRef *pdxlopWindowRef =
				CDXLScalarWindowRef::Cast(pdxlnPrElChild->GetOperator());
			CExpression *pexprScWindowFunc = Pexpr(pdxlnPrElChild);

			CScalar *popScalar = CScalar::PopConvert(pexprScWindowFunc->Pop());
			IMDId *mdid = popScalar->MdidType();
			const IMDType *pmdtype = m_pmda->RetrieveType(mdid);

			CName name(pdxlopPrEl->GetMdNameAlias()->GetMDName());

			// generate a new column reference
			CColRef *colref =
				m_pcf->PcrCreate(pmdtype, popScalar->TypeModifier(), name);
			CScalarProjectElement *popScPrEl =
				GPOS_NEW(m_mp) CScalarProjectElement(m_mp, colref);

			// store colid -> colref mapping
			BOOL fInserted GPOS_ASSERTS_ONLY = m_phmulcr->Insert(
				GPOS_NEW(m_mp) ULONG(pdxlopPrEl->Id()), colref);
			GPOS_ASSERT(fInserted);

			// generate a project element
			CExpression *pexprProjElem =
				GPOS_NEW(m_mp) CExpression(m_mp, popScPrEl, pexprScWindowFunc);

			// add the created project element to the project list of the window node
			ULONG ulSpecPos = pdxlopWindowRef->GetWindSpecPos();
			const CExpressionArray *pdrgpexpr =
				phmulpdrgpexpr->Find(&ulSpecPos);
			if (nullptr == pdrgpexpr)
			{
				CExpressionArray *pdrgpexprNew =
					GPOS_NEW(m_mp) CExpressionArray(m_mp);
				pdrgpexprNew->Append(pexprProjElem);
				BOOL fInsert GPOS_ASSERTS_ONLY = phmulpdrgpexpr->Insert(
					GPOS_NEW(m_mp) ULONG(ulSpecPos), pdrgpexprNew);
				GPOS_ASSERT(fInsert);
			}
			else
			{
				const_cast<CExpressionArray *>(pdrgpexpr)->Append(
					pexprProjElem);
			}
		}
	}

	// create the window operators (or when applicable a tree of window operators)
	CExpression *pexprLgSequence = nullptr;
	UlongToExprArrayMapIter hmiterulpdrgexpr(phmulpdrgpexpr);

	while (hmiterulpdrgexpr.Advance())
	{
		ULONG ulPos = *(hmiterulpdrgexpr.Key());
		CDXLWindowSpec *pdxlws = pdxlopWindow->GetWindowKeyAt(ulPos);

		const CExpressionArray *pdrgpexpr = hmiterulpdrgexpr.Value();
		GPOS_ASSERT(nullptr != pdrgpexpr);
		CScalarProjectList *popPrL = GPOS_NEW(m_mp) CScalarProjectList(m_mp);
		CExpression *pexprProjList = GPOS_NEW(m_mp) CExpression(
			m_mp, popPrL, const_cast<CExpressionArray *>(pdrgpexpr));

		CColRefArray *colref_array =
			PdrgpcrPartitionByCol(pdxlws->GetPartitionByColIdArray());
		CDistributionSpec *pds = nullptr;
		if (0 < colref_array->Size())
		{
			CExpressionArray *pdrgpexprScalarIdents =
				CUtils::PdrgpexprScalarIdents(m_mp, colref_array);
			pds = CDistributionSpecHashed::MakeHashedDistrSpec(
				m_mp, pdrgpexprScalarIdents, true /* fNullsCollocated */,
				nullptr /* pdshashedEquiv */, nullptr /* opfamilies */);
			if (nullptr == pds)
			{
				// FIXME: Handle PARTITION BY clauses that cannot be capture using CDistributionSpecHashed
				// CScalarProjectList uses CDistributionSpecHashed to represent PARTITION BY clauses, even
				// though the clause may use expressions that are not distributable. For now, ORCA falls
				// back for such cases.
				GPOS_RAISE(
					gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported,
					GPOS_WSZ_LIT(
						"no default hash opclasses found in window function"));
			}
		}
		else
		{
			// if no partition-by columns, window functions need gathered input
			pds = GPOS_NEW(m_mp) CDistributionSpecSingleton(
				CDistributionSpecSingleton::EstMaster);
		}
		colref_array->Release();

		CWindowFrameArray *pdrgpwf = GPOS_NEW(m_mp) CWindowFrameArray(m_mp);
		CWindowFrame *pwf = nullptr;
		if (nullptr != pdxlws->GetWindowFrame())
		{
			pwf = Pwf(pdxlws->GetWindowFrame());
		}
		else
		{
			// create an empty frame
			pwf = const_cast<CWindowFrame *>(CWindowFrame::PwfEmpty());
			pwf->AddRef();
		}
		pdrgpwf->Append(pwf);

		COrderSpecArray *pdrgpos = GPOS_NEW(m_mp) COrderSpecArray(m_mp);
		if (nullptr != pdxlws->GetSortColListDXL())
		{
			COrderSpec *pos = Pos(pdxlws->GetSortColListDXL());
			pdrgpos->Append(pos);
		}
		else
		{
			pdrgpos->Append(GPOS_NEW(m_mp) COrderSpec(m_mp));
		}

		CLogicalSequenceProject *popLgSequence =
			GPOS_NEW(m_mp) CLogicalSequenceProject(m_mp, pds, pdrgpos, pdrgpwf);
		pexprLgSequence = GPOS_NEW(m_mp)
			CExpression(m_mp, popLgSequence, pexprWindowChild, pexprProjList);
		pexprWindowChild = pexprLgSequence;
	}

	GPOS_ASSERT(nullptr != pexprLgSequence);

	// clean up
	phmulpdrgpexpr->Release();

	return pexprLgSequence;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PdrgpcrPartitionByCol
//
//	@doc:
// 		Create the array of column reference used in the partition by column
//		list of a window specification
//
//---------------------------------------------------------------------------
CColRefArray *
CTranslatorDXLToExpr::PdrgpcrPartitionByCol(
	const ULongPtrArray *partition_by_colid_array)
{
	const ULONG size = partition_by_colid_array->Size();
	CColRefArray *colref_array = GPOS_NEW(m_mp) CColRefArray(m_mp);
	for (ULONG ul = 0; ul < size; ul++)
	{
		const ULONG *pulColId = (*partition_by_colid_array)[ul];

		// get its column reference from the hash map
		CColRef *colref = LookupColRef(m_phmulcr, *pulColId);
		colref_array->Append(colref);
	}

	return colref_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::Pwf
//
//	@doc:
// 		Create a window frame from a DXL window frame node
//
//---------------------------------------------------------------------------
CWindowFrame *
CTranslatorDXLToExpr::Pwf(const CDXLWindowFrame *window_frame)
{
	CDXLNode *pdxlnTrail = window_frame->PdxlnTrailing();
	CDXLNode *pdxlnLead = window_frame->PdxlnLeading();

	CWindowFrame::EFrameBoundary efbLead =
		Efb(CDXLScalarWindowFrameEdge::Cast(pdxlnLead->GetOperator())
				->ParseDXLFrameBoundary());
	CWindowFrame::EFrameBoundary efbTrail =
		Efb(CDXLScalarWindowFrameEdge::Cast(pdxlnTrail->GetOperator())
				->ParseDXLFrameBoundary());

	CExpression *pexprTrail = nullptr;
	if (0 != pdxlnTrail->Arity())
	{
		pexprTrail = Pexpr((*pdxlnTrail)[0]);
	}

	CExpression *pexprLead = nullptr;
	if (0 != pdxlnLead->Arity())
	{
		pexprLead = Pexpr((*pdxlnLead)[0]);
	}

	CWindowFrame::EFrameExclusionStrategy efes =
		Efes(window_frame->ParseFrameExclusionStrategy());
	CWindowFrame::EFrameSpec efs = CWindowFrame::EfsRows;
	if (EdxlfsRange == window_frame->ParseDXLFrameSpec())
	{
		efs = CWindowFrame::EfsRange;
	}
	else if (EdxlfsGroups == window_frame->ParseDXLFrameSpec())
	{
		efs = CWindowFrame::EfsGroups;
	}

	CWindowFrame *pwf = GPOS_NEW(m_mp) CWindowFrame(
		m_mp, efs, efbLead, efbTrail, pexprLead, pexprTrail, efes,
		window_frame->PdxlnStartInRangeFunc(),
		window_frame->PdxlnEndInRangeFunc(), window_frame->PdxlnInRangeColl(),
		window_frame->PdxlnInRangeAsc(),
		window_frame->PdxlnInRangeNullsFirst());

	return pwf;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::Efb
//
//	@doc:
//		Return the window frame boundary
//
//---------------------------------------------------------------------------
CWindowFrame::EFrameBoundary
CTranslatorDXLToExpr::Efb(EdxlFrameBoundary frame_boundary)
{
	ULONG window_frame_boundary_to_frame_boundary_mapping[][2] = {
		{EdxlfbUnboundedPreceding, CWindowFrame::EfbUnboundedPreceding},
		{EdxlfbBoundedPreceding, CWindowFrame::EfbBoundedPreceding},
		{EdxlfbCurrentRow, CWindowFrame::EfbCurrentRow},
		{EdxlfbUnboundedFollowing, CWindowFrame::EfbUnboundedFollowing},
		{EdxlfbBoundedFollowing, CWindowFrame::EfbBoundedFollowing},
		{EdxlfbDelayedBoundedPreceding,
		 CWindowFrame::EfbDelayedBoundedPreceding},
		{EdxlfbDelayedBoundedFollowing,
		 CWindowFrame::EfbDelayedBoundedFollowing}};

#ifdef GPOS_DEBUG
	const ULONG arity =
		GPOS_ARRAY_SIZE(window_frame_boundary_to_frame_boundary_mapping);
	GPOS_ASSERT(arity > (ULONG) frame_boundary &&
				"Invalid window frame boundary");
#endif
	CWindowFrame::EFrameBoundary efb = (CWindowFrame::EFrameBoundary)
		window_frame_boundary_to_frame_boundary_mapping[(ULONG) frame_boundary]
													   [1];

	return efb;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::Efes
//
//	@doc:
//		Return the window frame exclusion strategy
//
//---------------------------------------------------------------------------
CWindowFrame::EFrameExclusionStrategy
CTranslatorDXLToExpr::Efes(EdxlFrameExclusionStrategy edxlfeb)
{
	ULONG window_frame_boundary_to_frame_boundary_mapping[][2] = {
		{EdxlfesNone, CWindowFrame::EfesNone},
		{EdxlfesNulls, CWindowFrame::EfesNulls},
		{EdxlfesCurrentRow, CWindowFrame::EfesCurrentRow},
		{EdxlfesGroup, CWindowFrame::EfseMatchingOthers},
		{EdxlfesTies, CWindowFrame::EfesTies}};

#ifdef GPOS_DEBUG
	const ULONG arity =
		GPOS_ARRAY_SIZE(window_frame_boundary_to_frame_boundary_mapping);
	GPOS_ASSERT(arity > (ULONG) edxlfeb &&
				"Invalid window frame exclusion strategy");
#endif
	CWindowFrame::EFrameExclusionStrategy efeb =
		(CWindowFrame::EFrameExclusionStrategy)
			window_frame_boundary_to_frame_boundary_mapping[(ULONG) edxlfeb][1];

	return efeb;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprLogicalJoin
//
//	@doc:
// 		Create a logical join expr from a DXL logical join
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprLogicalJoin(const CDXLNode *dxlnode)
{
	GPOS_ASSERT(nullptr != dxlnode);

	CDXLLogicalJoin *pdxlopJoin = CDXLLogicalJoin::Cast(dxlnode->GetOperator());
	EdxlJoinType join_type = pdxlopJoin->GetJoinType();

	if (EdxljtRight == join_type)
	{
		return PexprRightOuterJoin(dxlnode);
	}

	if (EdxljtInner != join_type && EdxljtLeft != join_type &&
		EdxljtFull != join_type)
	{
		GPOS_RAISE(gpopt::ExmaGPOPT, gpopt::ExmiUnsupportedOp,
				   CDXLOperator::GetJoinTypeNameStr(pdxlopJoin->GetJoinType())
					   ->GetBuffer());
	}

	CExpressionArray *pdrgpexprChildren = GPOS_NEW(m_mp) CExpressionArray(m_mp);

	const ULONG ulChildCount = dxlnode->Arity();
	for (ULONG ul = 0; ul < ulChildCount - 1; ++ul)
	{
		// get the next child dxl node and then translate it into an Expr
		CDXLNode *pdxlnNxtChild = (*dxlnode)[ul];

		CExpression *pexprNxtChild = PexprLogical(pdxlnNxtChild);
		pdrgpexprChildren->Append(pexprNxtChild);
	}

	// get the scalar condition and then translate it
	CDXLNode *pdxlnCond = (*dxlnode)[ulChildCount - 1];
	CExpression *pexprCond = PexprScalar(pdxlnCond);
	pdrgpexprChildren->Append(pexprCond);

	return CUtils::PexprLogicalJoin(m_mp, join_type, pdrgpexprChildren);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprRightOuterJoin
//
//	@doc:
// 		Translate a DXL right outer join. The expression A ROJ B is translated
//		to: B LOJ A
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprRightOuterJoin(const CDXLNode *dxlnode)
{
#ifdef GPOS_DEBUG
	CDXLLogicalJoin *pdxlopJoin = CDXLLogicalJoin::Cast(dxlnode->GetOperator());
	const ULONG ulChildCount = dxlnode->Arity();
#endif	//GPOS_DEBUG
	GPOS_ASSERT(EdxljtRight == pdxlopJoin->GetJoinType() && 3 == ulChildCount);

	CExpressionArray *pdrgpexprChildren = GPOS_NEW(m_mp) CExpressionArray(m_mp);
	pdrgpexprChildren->Append(PexprLogical((*dxlnode)[1]));
	pdrgpexprChildren->Append(PexprLogical((*dxlnode)[0]));
	pdrgpexprChildren->Append(PexprScalar((*dxlnode)[2]));

	return CUtils::PexprLogicalJoin(m_mp, EdxljtLeft, pdrgpexprChildren);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::Ptabdesc
//
//	@doc:
//		Construct a table descriptor from DXL table descriptor
//
//---------------------------------------------------------------------------
CTableDescriptor *
CTranslatorDXLToExpr::Ptabdesc(CDXLTableDescr *table_descr)
{
	CWStringConst strName(m_mp,
						  table_descr->MdName()->GetMDName()->GetBuffer());

	IMDId *mdid = table_descr->MDId();

	// get the relation information from the cache
	const IMDRelation *pmdrel = m_pmda->RetrieveRel(mdid);

	// construct mappings for columns that are not dropped
	IntToUlongMap *phmiulAttnoColMapping = GPOS_NEW(m_mp) IntToUlongMap(m_mp);
	IntToUlongMap *phmiulAttnoPosMapping = GPOS_NEW(m_mp) IntToUlongMap(m_mp);
	UlongToUlongMap *phmululColMapping = GPOS_NEW(m_mp) UlongToUlongMap(m_mp);

	const ULONG ulAllColumns = pmdrel->ColumnCount();
	ULONG ulPosNonDropped = 0;
	for (ULONG ulPos = 0; ulPos < ulAllColumns; ulPos++)
	{
		const IMDColumn *pmdcol = pmdrel->GetMdCol(ulPos);
		if (pmdcol->IsDropped())
		{
			continue;
		}
		(void) phmiulAttnoColMapping->Insert(
			GPOS_NEW(m_mp) INT(pmdcol->AttrNum()),
			GPOS_NEW(m_mp) ULONG(ulPosNonDropped));
		(void) phmiulAttnoPosMapping->Insert(
			GPOS_NEW(m_mp) INT(pmdcol->AttrNum()), GPOS_NEW(m_mp) ULONG(ulPos));
		(void) phmululColMapping->Insert(GPOS_NEW(m_mp) ULONG(ulPos),
										 GPOS_NEW(m_mp) ULONG(ulPosNonDropped));

		ulPosNonDropped++;
	}

	// get distribution policy
	IMDRelation::Ereldistrpolicy rel_distr_policy =
		pmdrel->GetRelDistribution();

	// get storage type
	IMDRelation::Erelstoragetype rel_storage_type =
		pmdrel->RetrieveRelStorageType();

	mdid->AddRef();
	CTableDescriptor *ptabdesc = GPOS_NEW(m_mp) CTableDescriptor(
		m_mp, mdid, CName(m_mp, &strName), pmdrel->ConvertHashToRandom(),
		rel_distr_policy, rel_storage_type, table_descr->GetExecuteAsUserId(),
		table_descr->LockMode(), table_descr->GetAssignedQueryIdForTargetRel());

	const ULONG ulColumns = table_descr->Arity();
	for (ULONG ul = 0; ul < ulColumns; ul++)
	{
		const CDXLColDescr *pdxlcoldesc = table_descr->GetColumnDescrAt(ul);
		INT attno = pdxlcoldesc->AttrNum();

		ULONG *pulPos = phmiulAttnoPosMapping->Find(&attno);
		GPOS_ASSERT(nullptr != pulPos);
		const IMDColumn *pmdcolNext = pmdrel->GetMdCol(*pulPos);

		BOOL is_nullable = pmdcolNext->IsNullable();

		GPOS_ASSERT(pdxlcoldesc->MdidType()->IsValid());
		const IMDType *pmdtype = m_pmda->RetrieveType(pdxlcoldesc->MdidType());

		GPOS_ASSERT(nullptr != pdxlcoldesc->MdName()->GetMDName()->GetBuffer());
		CWStringConst strColName(
			m_mp, pdxlcoldesc->MdName()->GetMDName()->GetBuffer());

		INT attrnum = pdxlcoldesc->AttrNum();

		const ULONG ulWidth = pdxlcoldesc->Width();
		CColumnDescriptor *pcoldesc = GPOS_NEW(m_mp) CColumnDescriptor(
			m_mp, pmdtype, pdxlcoldesc->TypeModifier(),
			CName(m_mp, &strColName), attrnum, is_nullable, ulWidth);

		ptabdesc->AddColumn(pcoldesc);
	}

	if (IMDRelation::EreldistrHash == rel_distr_policy)
	{
		AddDistributionColumns(ptabdesc, pmdrel, phmiulAttnoColMapping);
	}

	if (pmdrel->IsPartitioned())
	{
		const ULONG ulPartCols = pmdrel->PartColumnCount();
		// compute partition columns for table descriptor
		for (ULONG ul = 0; ul < ulPartCols; ul++)
		{
			const IMDColumn *pmdcol = pmdrel->PartColAt(ul);
			INT attrnum = pmdcol->AttrNum();
			ULONG *pulPos = phmiulAttnoColMapping->Find(&attrnum);
			GPOS_ASSERT(nullptr != pulPos);
			ptabdesc->AddPartitionColumn(*pulPos);
		}
	}

	// populate key sets
	CTranslatorDXLToExprUtils::AddKeySets(m_mp, ptabdesc, pmdrel,
										  phmululColMapping);

	phmiulAttnoPosMapping->Release();
	phmiulAttnoColMapping->Release();
	phmululColMapping->Release();

	if (IMDRelation::EreldistrMasterOnly == rel_distr_policy)
	{
		COptCtxt::PoctxtFromTLS()->SetHasMasterOnlyTables();
	}

	if (IMDRelation::EreldistrReplicated == rel_distr_policy)
	{
		COptCtxt::PoctxtFromTLS()->SetHasReplicatedTables();
	}

	return ptabdesc;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::RegisterMDRelationCtas
//
//	@doc:
//		Register the MD relation entry for the given CTAS operator
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToExpr::RegisterMDRelationCtas(CDXLLogicalCTAS *pdxlopCTAS)
{
	GPOS_ASSERT(nullptr != pdxlopCTAS);

	pdxlopCTAS->MDId()->AddRef();

	if (nullptr != pdxlopCTAS->GetDistrColPosArray())
	{
		pdxlopCTAS->GetDistrColPosArray()->AddRef();
	}
	pdxlopCTAS->GetDxlCtasStorageOption()->AddRef();

	CMDColumnArray *mdcol_array = GPOS_NEW(m_mp) CMDColumnArray(m_mp);
	CDXLColDescrArray *dxl_col_descr_array =
		pdxlopCTAS->GetDXLColumnDescrArray();
	const ULONG length = dxl_col_descr_array->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CDXLColDescr *pdxlcd = (*dxl_col_descr_array)[ul];
		pdxlcd->MdidType()->AddRef();

		CMDColumn *pmdcol = GPOS_NEW(m_mp) CMDColumn(
			GPOS_NEW(m_mp) CMDName(m_mp, pdxlcd->MdName()->GetMDName()),
			pdxlcd->AttrNum(), pdxlcd->MdidType(), pdxlcd->TypeModifier(),
			true,  // is_nullable,
			pdxlcd->IsDropped(),
			nullptr,  // pdxlnDefaultValue,
			pdxlcd->Width());
		mdcol_array->Append(pmdcol);
	}

	CMDName *mdname_schema = nullptr;
	if (nullptr != pdxlopCTAS->GetMdNameSchema())
	{
		mdname_schema = GPOS_NEW(m_mp)
			CMDName(m_mp, pdxlopCTAS->GetMdNameSchema()->GetMDName());
	}

	IntPtrArray *vartypemod_array = pdxlopCTAS->GetVarTypeModArray();
	vartypemod_array->AddRef();

	IMdIdArray *distr_opfamilies = pdxlopCTAS->GetDistrOpfamilies();
	distr_opfamilies->AddRef();
	IMdIdArray *distr_opclasses = pdxlopCTAS->GetDistrOpclasses();
	distr_opclasses->AddRef();

	CMDRelationCtasGPDB *pmdrel = GPOS_NEW(m_mp) CMDRelationCtasGPDB(
		m_mp, pdxlopCTAS->MDId(), mdname_schema,
		GPOS_NEW(m_mp) CMDName(m_mp, pdxlopCTAS->MdName()->GetMDName()),
		pdxlopCTAS->IsTemporary(), pdxlopCTAS->RetrieveRelStorageType(),
		pdxlopCTAS->Ereldistrpolicy(), mdcol_array,
		pdxlopCTAS->GetDistrColPosArray(), distr_opfamilies, distr_opclasses,
		GPOS_NEW(m_mp) ULongPtr2dArray(m_mp),  // keyset_array,
		pdxlopCTAS->GetDxlCtasStorageOption(), vartypemod_array);

	IMDCacheObjectArray *mdcache_obj_array =
		GPOS_NEW(m_mp) IMDCacheObjectArray(m_mp);
	mdcache_obj_array->Append(pmdrel);
	CMDProviderMemory *pmdp =
		GPOS_NEW(m_mp) CMDProviderMemory(m_mp, mdcache_obj_array);
	m_pmda->RegisterProvider(pdxlopCTAS->MDId()->Sysid(), pmdp);

	// cleanup
	mdcache_obj_array->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PtabdescFromCTAS
//
//	@doc:
//		Construct a table descriptor for a CTAS operator
//
//---------------------------------------------------------------------------
CTableDescriptor *
CTranslatorDXLToExpr::PtabdescFromCTAS(CDXLLogicalCTAS *pdxlopCTAS)
{
	CWStringConst strName(m_mp, pdxlopCTAS->MdName()->GetMDName()->GetBuffer());

	IMDId *mdid = pdxlopCTAS->MDId();

	// get the relation information from the cache
	const IMDRelation *pmdrel = m_pmda->RetrieveRel(mdid);

	// construct mappings for columns that are not dropped
	IntToUlongMap *phmiulAttnoColMapping = GPOS_NEW(m_mp) IntToUlongMap(m_mp);
	UlongToUlongMap *phmululColMapping = GPOS_NEW(m_mp) UlongToUlongMap(m_mp);

	const ULONG ulAllColumns = pmdrel->ColumnCount();
	ULONG ulPosNonDropped = 0;
	for (ULONG ulPos = 0; ulPos < ulAllColumns; ulPos++)
	{
		const IMDColumn *pmdcol = pmdrel->GetMdCol(ulPos);
		if (pmdcol->IsDropped())
		{
			continue;
		}
		(void) phmiulAttnoColMapping->Insert(
			GPOS_NEW(m_mp) INT(pmdcol->AttrNum()),
			GPOS_NEW(m_mp) ULONG(ulPosNonDropped));
		(void) phmululColMapping->Insert(GPOS_NEW(m_mp) ULONG(ulPos),
										 GPOS_NEW(m_mp) ULONG(ulPosNonDropped));

		ulPosNonDropped++;
	}

	// get distribution policy
	IMDRelation::Ereldistrpolicy rel_distr_policy =
		pmdrel->GetRelDistribution();

	// get storage type
	IMDRelation::Erelstoragetype rel_storage_type =
		pmdrel->RetrieveRelStorageType();

	mdid->AddRef();
	CTableDescriptor *ptabdesc = GPOS_NEW(m_mp) CTableDescriptor(
		m_mp, mdid, CName(m_mp, &strName), pmdrel->ConvertHashToRandom(),
		rel_distr_policy, rel_storage_type,
		0,	// ulExecuteAsUser, use permissions of current user
		3,	// CTEs always use a RowExclusiveLock on the table. See createas.c
		UNASSIGNED_QUERYID);

	// populate column information from the dxl table descriptor
	CDXLColDescrArray *dxl_col_descr_array =
		pdxlopCTAS->GetDXLColumnDescrArray();
	const ULONG ulColumns = dxl_col_descr_array->Size();
	for (ULONG ul = 0; ul < ulColumns; ul++)
	{
		BOOL is_nullable = false;
		if (ul < pmdrel->ColumnCount())
		{
			is_nullable = pmdrel->GetMdCol(ul)->IsNullable();
		}

		const CDXLColDescr *pdxlcoldesc = (*dxl_col_descr_array)[ul];

		GPOS_ASSERT(pdxlcoldesc->MdidType()->IsValid());
		const IMDType *pmdtype = m_pmda->RetrieveType(pdxlcoldesc->MdidType());

		GPOS_ASSERT(nullptr != pdxlcoldesc->MdName()->GetMDName()->GetBuffer());
		CWStringConst strColName(
			m_mp, pdxlcoldesc->MdName()->GetMDName()->GetBuffer());

		INT attrnum = pdxlcoldesc->AttrNum();

		const ULONG ulWidth = pdxlcoldesc->Width();
		CColumnDescriptor *pcoldesc = GPOS_NEW(m_mp) CColumnDescriptor(
			m_mp, pmdtype, pdxlcoldesc->TypeModifier(),
			CName(m_mp, &strColName), attrnum, is_nullable, ulWidth);

		ptabdesc->AddColumn(pcoldesc);
	}

	if (IMDRelation::EreldistrHash == rel_distr_policy)
	{
		AddDistributionColumns(ptabdesc, pmdrel, phmiulAttnoColMapping);
	}

	GPOS_ASSERT(!pmdrel->IsPartitioned());

	phmiulAttnoColMapping->Release();
	phmululColMapping->Release();

	return ptabdesc;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarSubqueryExistential
//
//	@doc:
// 		Translate existential subquery
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarSubqueryExistential(
	Edxlopid edxlopid, CDXLNode *pdxlnLogicalChild)
{
	GPOS_ASSERT(EdxlopScalarSubqueryExists == edxlopid ||
				EdxlopScalarSubqueryNotExists == edxlopid);
	GPOS_ASSERT(nullptr != pdxlnLogicalChild);

	CExpression *pexprLogicalChild = Pexpr(pdxlnLogicalChild);
	GPOS_ASSERT(nullptr != pexprLogicalChild);

	CScalar *popScalarSubquery = nullptr;
	if (EdxlopScalarSubqueryExists == edxlopid)
	{
		popScalarSubquery = GPOS_NEW(m_mp) CScalarSubqueryExists(m_mp);
	}
	else
	{
		popScalarSubquery = GPOS_NEW(m_mp) CScalarSubqueryNotExists(m_mp);
	}

	return GPOS_NEW(m_mp)
		CExpression(m_mp, popScalarSubquery, pexprLogicalChild);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprLogicalConstTableGet
//
//	@doc:
// 		Create a logical const table get expression from the corresponding
//		DXL node
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprLogicalConstTableGet(const CDXLNode *pdxlnConstTable)
{
	CDXLLogicalConstTable *pdxlopConstTable =
		CDXLLogicalConstTable::Cast(pdxlnConstTable->GetOperator());

	const CDXLColDescrArray *dxl_col_descr_array =
		pdxlopConstTable->GetDXLColumnDescrArray();

	// translate the column descriptors
	CColumnDescriptorArray *pdrgpcoldesc =
		GPOS_NEW(m_mp) CColumnDescriptorArray(m_mp);
	const ULONG ulColumns = dxl_col_descr_array->Size();

	for (ULONG ulColIdx = 0; ulColIdx < ulColumns; ulColIdx++)
	{
		CDXLColDescr *pdxlcd = (*dxl_col_descr_array)[ulColIdx];
		const IMDType *pmdtype = m_pmda->RetrieveType(pdxlcd->MdidType());
		CName name(m_mp, pdxlcd->MdName()->GetMDName());

		const ULONG ulWidth = pdxlcd->Width();
		CColumnDescriptor *pcoldesc = GPOS_NEW(m_mp)
			CColumnDescriptor(m_mp, pmdtype, pdxlcd->TypeModifier(), name,
							  ulColIdx + 1,	 // attno
							  true,			 // IsNullable
							  ulWidth);
		pdrgpcoldesc->Append(pcoldesc);
	}

	// translate values
	IDatum2dArray *pdrgpdrgpdatum = GPOS_NEW(m_mp) IDatum2dArray(m_mp);

	const ULONG ulValues = pdxlopConstTable->GetConstTupleCount();
	for (ULONG ul = 0; ul < ulValues; ul++)
	{
		const CDXLDatumArray *pdrgpdxldatum =
			pdxlopConstTable->GetConstTupleDatumArrayAt(ul);
		IDatumArray *pdrgpdatum =
			CTranslatorDXLToExprUtils::Pdrgpdatum(m_mp, m_pmda, pdrgpdxldatum);
		pdrgpdrgpdatum->Append(pdrgpdatum);
	}

	// create a logical const table get operator
	CLogicalConstTableGet *popConstTableGet = GPOS_NEW(m_mp)
		CLogicalConstTableGet(m_mp, pdrgpcoldesc, pdrgpdrgpdatum);

	// construct the mapping between the DXL ColId and CColRef
	ConstructDXLColId2ColRefMapping(pdxlopConstTable->GetDXLColumnDescrArray(),
									popConstTableGet->PdrgpcrOutput());

	return GPOS_NEW(m_mp) CExpression(m_mp, popConstTableGet);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarSubqueryQuantified
//
//	@doc:
// 		Helper for creating quantified subquery
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarSubqueryQuantified(
	Edxlopid edxlopid, IMDId *scalar_op_mdid, const CWStringConst *str,
	ULONG colid, CDXLNode *pdxlnLogicalChild, CDXLNode *pdxlnScalarChild)
{
	GPOS_ASSERT(EdxlopScalarSubqueryAny == edxlopid ||
				EdxlopScalarSubqueryAll == edxlopid);
	GPOS_ASSERT(nullptr != str);
	GPOS_ASSERT(nullptr != pdxlnLogicalChild);
	GPOS_ASSERT(nullptr != pdxlnScalarChild);

	// translate children

	CExpression *pexprLogicalChild = Pexpr(pdxlnLogicalChild);
	CExpression *pexprScalarChild = Pexpr(pdxlnScalarChild);

	// get colref for subquery colid
	const CColRef *colref = LookupColRef(m_phmulcr, colid);

	CScalar *popScalarSubquery = nullptr;
	if (EdxlopScalarSubqueryAny == edxlopid)
	{
		popScalarSubquery = GPOS_NEW(m_mp) CScalarSubqueryAny(
			m_mp, scalar_op_mdid,
			GPOS_NEW(m_mp) CWStringConst(m_mp, str->GetBuffer()), colref);
	}
	else
	{
		popScalarSubquery = GPOS_NEW(m_mp) CScalarSubqueryAll(
			m_mp, scalar_op_mdid,
			GPOS_NEW(m_mp) CWStringConst(m_mp, str->GetBuffer()), colref);
	}

	// create a scalar subquery any expression with the relational expression as
	// first child and the scalar expression as second child
	CExpression *pexpr = GPOS_NEW(m_mp) CExpression(
		m_mp, popScalarSubquery, pexprLogicalChild, pexprScalarChild);

	return pexpr;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarSubqueryQuantified
//
//	@doc:
// 		Create a quantified subquery from a DXL node
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarSubqueryQuantified(
	const CDXLNode *pdxlnSubquery)
{
	GPOS_ASSERT(nullptr != pdxlnSubquery);

	CDXLScalar *dxl_op =
		dynamic_cast<CDXLScalar *>(pdxlnSubquery->GetOperator());
	GPOS_ASSERT(nullptr != dxl_op);

	CDXLScalarSubqueryQuantified *pdxlopSubqueryQuantified =
		CDXLScalarSubqueryQuantified::Cast(pdxlnSubquery->GetOperator());
	GPOS_ASSERT(nullptr != pdxlopSubqueryQuantified);

	IMDId *mdid = pdxlopSubqueryQuantified->GetScalarOpMdId();
	mdid->AddRef();
	return PexprScalarSubqueryQuantified(
		dxl_op->GetDXLOperator(), mdid,
		pdxlopSubqueryQuantified->GetScalarOpMdName()->GetMDName(),
		pdxlopSubqueryQuantified->GetColId(),
		(*pdxlnSubquery)
			[CDXLScalarSubqueryQuantified::EdxlsqquantifiedIndexRelational],
		(*pdxlnSubquery)
			[CDXLScalarSubqueryQuantified::EdxlsqquantifiedIndexScalar]);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalar
//
//	@doc:
// 		Create a logical select expr from a DXL logical select
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalar(const CDXLNode *dxlnode)
{
	GPOS_ASSERT(nullptr != dxlnode);
	GPOS_ASSERT(EdxloptypeScalar ==
				dxlnode->GetOperator()->GetDXLOperatorType());
	CDXLOperator *dxl_op = dxlnode->GetOperator();
	ULONG ulOpId = (ULONG) dxl_op->GetDXLOperator();

	if (EdxlopScalarSubqueryExists == ulOpId ||
		EdxlopScalarSubqueryNotExists == ulOpId)
	{
		return PexprScalarSubqueryExistential(
			dxlnode->GetOperator()->GetDXLOperator(), (*dxlnode)[0]);
	}

	switch (dxl_op->GetDXLOperator())
	{
		case EdxlopScalarIdent:
			return CTranslatorDXLToExpr::PexprScalarIdent(dxlnode);
		case EdxlopScalarCmp:
			return CTranslatorDXLToExpr::PexprScalarCmp(dxlnode);
		case EdxlopScalarOpExpr:
			return CTranslatorDXLToExpr::PexprScalarOp(dxlnode);
		case EdxlopScalarDistinct:
			return CTranslatorDXLToExpr::PexprScalarIsDistinctFrom(dxlnode);
		case EdxlopScalarConstValue:
			return CTranslatorDXLToExpr::PexprScalarConst(dxlnode);
		case EdxlopScalarBoolExpr:
			return CTranslatorDXLToExpr::PexprScalarBoolOp(dxlnode);
		case EdxlopScalarFuncExpr:
			return CTranslatorDXLToExpr::PexprScalarFunc(dxlnode);
		case EdxlopScalarMinMax:
			return CTranslatorDXLToExpr::PexprScalarMinMax(dxlnode);
		case EdxlopScalarAggref:
			return CTranslatorDXLToExpr::PexprAggFunc(dxlnode);
		case EdxlopScalarWindowRef:
			return CTranslatorDXLToExpr::PexprWindowFunc(dxlnode);
		case EdxlopScalarNullTest:
			return CTranslatorDXLToExpr::PexprScalarNullTest(dxlnode);
		case EdxlopScalarNullIf:
			return CTranslatorDXLToExpr::PexprScalarNullIf(dxlnode);
		case EdxlopScalarBooleanTest:
			return CTranslatorDXLToExpr::PexprScalarBooleanTest(dxlnode);
		case EdxlopScalarIfStmt:
			return CTranslatorDXLToExpr::PexprScalarIf(dxlnode);
		case EdxlopScalarSwitch:
			return CTranslatorDXLToExpr::PexprScalarSwitch(dxlnode);
		case EdxlopScalarSwitchCase:
			return CTranslatorDXLToExpr::PexprScalarSwitchCase(dxlnode);
		case EdxlopScalarCaseTest:
			return CTranslatorDXLToExpr::PexprScalarCaseTest(dxlnode);
		case EdxlopScalarCoalesce:
			return CTranslatorDXLToExpr::PexprScalarCoalesce(dxlnode);
		case EdxlopScalarArrayCoerceExpr:
			return CTranslatorDXLToExpr::PexprScalarArrayCoerceExpr(dxlnode);
		case EdxlopScalarCast:
			return CTranslatorDXLToExpr::PexprScalarCast(dxlnode);
		case EdxlopScalarCoerceToDomain:
			return CTranslatorDXLToExpr::PexprScalarCoerceToDomain(dxlnode);
		case EdxlopScalarCoerceViaIO:
			return CTranslatorDXLToExpr::PexprScalarCoerceViaIO(dxlnode);
		case EdxlopScalarSubquery:
			return CTranslatorDXLToExpr::PexprScalarSubquery(dxlnode);
		case EdxlopScalarSubqueryAny:
		case EdxlopScalarSubqueryAll:
			return CTranslatorDXLToExpr::PexprScalarSubqueryQuantified(dxlnode);
		case EdxlopScalarArray:
			return CTranslatorDXLToExpr::PexprArray(dxlnode);
		case EdxlopScalarArrayComp:
			return CTranslatorDXLToExpr::PexprArrayCmp(dxlnode);
		case EdxlopScalarArrayRef:
			return CTranslatorDXLToExpr::PexprArrayRef(dxlnode);
		case EdxlopScalarArrayRefIndexList:
			return CTranslatorDXLToExpr::PexprArrayRefIndexList(dxlnode);
		case EdxlopScalarValuesList:
			return CTranslatorDXLToExpr::PexprValuesList(dxlnode);
		case EdxlopScalarSortGroupClause:
			return CTranslatorDXLToExpr::PexprSortGroupClause(dxlnode);
		default:
			GPOS_RAISE(gpopt::ExmaGPOPT, gpopt::ExmiUnsupportedOp,
					   dxl_op->GetOpNameStr()->GetBuffer());
			return nullptr;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprCollapseNot
//
//	@doc:
// 		Collapse a NOT node by looking at its child.
//		Return NULL if it is not collapsible.
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprCollapseNot(const CDXLNode *pdxlnNotExpr)
{
	GPOS_ASSERT(nullptr != pdxlnNotExpr);
	GPOS_ASSERT(CTranslatorDXLToExprUtils::FScalarBool(pdxlnNotExpr, Edxlnot));

	CDXLNode *pdxlnNotChild = (*pdxlnNotExpr)[0];

	if (CTranslatorDXLToExprUtils::FScalarBool(pdxlnNotChild, Edxlnot))
	{
		// two cascaded NOT nodes cancel each other
		return Pexpr((*pdxlnNotChild)[0]);
	}

	Edxlopid edxlopid = pdxlnNotChild->GetOperator()->GetDXLOperator();
	if (EdxlopScalarSubqueryExists == edxlopid ||
		EdxlopScalarSubqueryNotExists == edxlopid)
	{
		// NOT followed by EXISTS/NOTEXISTS is translated as NOTEXISTS/EXISTS
		Edxlopid edxlopidNew = (EdxlopScalarSubqueryExists == edxlopid)
								   ? EdxlopScalarSubqueryNotExists
								   : EdxlopScalarSubqueryExists;

		return PexprScalarSubqueryExistential(edxlopidNew, (*pdxlnNotChild)[0]);
	}

	if (EdxlopScalarSubqueryAny == edxlopid ||
		EdxlopScalarSubqueryAll == edxlopid)
	{
		// NOT followed by ANY/ALL<op> is translated as ALL/ANY<inverse_op>
		CDXLScalarSubqueryQuantified *pdxlopSubqueryQuantified =
			CDXLScalarSubqueryQuantified::Cast(pdxlnNotChild->GetOperator());
		Edxlopid edxlopidNew = (EdxlopScalarSubqueryAny == edxlopid)
								   ? EdxlopScalarSubqueryAll
								   : EdxlopScalarSubqueryAny;

		// get mdid and name of the inverse of the comparison operator used by quantified subquery
		IMDId *mdid_op = pdxlopSubqueryQuantified->GetScalarOpMdId();
		IMDId *pmdidInverseOp =
			m_pmda->RetrieveScOp(mdid_op)->GetInverseOpMdid();

		// if inverse operator cannot be found in metadata, the optimizer won't collapse NOT node
		if (nullptr == pmdidInverseOp)
		{
			return nullptr;
		}

		const CWStringConst *pstrInverseOp =
			m_pmda->RetrieveScOp(pmdidInverseOp)->Mdname().GetMDName();

		pmdidInverseOp->AddRef();
		return PexprScalarSubqueryQuantified(
			edxlopidNew, pmdidInverseOp, pstrInverseOp,
			pdxlopSubqueryQuantified->GetColId(),
			(*pdxlnNotChild)
				[CDXLScalarSubqueryQuantified::EdxlsqquantifiedIndexRelational],
			(*pdxlnNotChild)
				[CDXLScalarSubqueryQuantified::EdxlsqquantifiedIndexScalar]);
	}

	// collapsing NOT node failed
	return nullptr;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarBoolOp
//
//	@doc:
// 		Create a scalar logical op representation in the optimizer
//		from a DXL scalar boolean expr
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarBoolOp(const CDXLNode *pdxlnBoolExpr)
{
	GPOS_ASSERT(nullptr != pdxlnBoolExpr);

	EdxlBoolExprType edxlbooltype =
		CDXLScalarBoolExpr::Cast(pdxlnBoolExpr->GetOperator())
			->GetDxlBoolTypeStr();

	GPOS_ASSERT((edxlbooltype == Edxlnot) || (edxlbooltype == Edxlor) ||
				(edxlbooltype == Edxland));
	GPOS_ASSERT_IMP(Edxlnot == edxlbooltype, 1 == pdxlnBoolExpr->Arity());

	if (Edxlnot == edxlbooltype)
	{
		// attempt collapsing NOT node
		CExpression *pexprResult = PexprCollapseNot(pdxlnBoolExpr);
		if (nullptr != pexprResult)
		{
			return pexprResult;
		}
	}

	CScalarBoolOp::EBoolOperator eboolop =
		CTranslatorDXLToExprUtils::EBoolOperator(edxlbooltype);

	CExpressionArray *pdrgpexprChildren = PdrgpexprChildren(pdxlnBoolExpr);

	return CUtils::PexprScalarBoolOp(m_mp, eboolop, pdrgpexprChildren);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarOp
//
//	@doc:
// 		Create a scalar operation from a DXL scalar op expr
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarOp(const CDXLNode *pdxlnOpExpr)
{
	// TODO: Aug 22 2011; In GPDB the opexpr can only have two children. However, in other
	// databases this may not be the case
	GPOS_ASSERT(nullptr != pdxlnOpExpr &&
				(1 == pdxlnOpExpr->Arity() || (2 == pdxlnOpExpr->Arity())));

	CDXLScalarOpExpr *dxl_op =
		CDXLScalarOpExpr::Cast(pdxlnOpExpr->GetOperator());

	CExpressionArray *pdrgpexprArgs = PdrgpexprChildren(pdxlnOpExpr);

	IMDId *mdid = dxl_op->MDId();
	mdid->AddRef();

	IMDId *return_type_mdid = dxl_op->GetReturnTypeMdId();
	if (nullptr != return_type_mdid)
	{
		return_type_mdid->AddRef();
	}
	CScalarOp *pscop = GPOS_NEW(m_mp) CScalarOp(
		m_mp, mdid, return_type_mdid,
		GPOS_NEW(m_mp)
			CWStringConst(m_mp, dxl_op->GetScalarOpNameStr()->GetBuffer()));

	CExpression *pexpr = GPOS_NEW(m_mp) CExpression(m_mp, pscop, pdrgpexprArgs);

	return pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarIsDistinctFrom
//
//	@doc:
// 		Create a scalar distinct expr from a DXL scalar distinct compare
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarIsDistinctFrom(const CDXLNode *pdxlnDistCmp)
{
	GPOS_ASSERT(nullptr != pdxlnDistCmp && 2 == pdxlnDistCmp->Arity());
	CDXLScalarDistinctComp *pdxlopDistCmp =
		CDXLScalarDistinctComp::Cast(pdxlnDistCmp->GetOperator());
	// get children
	CDXLNode *dxlnode_left = (*pdxlnDistCmp)[0];
	CDXLNode *dxlnode_right = (*pdxlnDistCmp)[1];

	// translate left and right children
	CExpression *pexprLeft = Pexpr(dxlnode_left);
	CExpression *pexprRight = Pexpr(dxlnode_right);

	IMDId *mdid_op = pdxlopDistCmp->MDId();
	mdid_op->AddRef();
	const IMDScalarOp *md_scalar_op = m_pmda->RetrieveScOp(mdid_op);

	CScalarIsDistinctFrom *popScIDF = GPOS_NEW(m_mp) CScalarIsDistinctFrom(
		m_mp, mdid_op,
		GPOS_NEW(m_mp) CWStringConst(
			m_mp, (md_scalar_op->Mdname().GetMDName())->GetBuffer()));

	CExpression *pexpr =
		GPOS_NEW(m_mp) CExpression(m_mp, popScIDF, pexprLeft, pexprRight);

	return pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarNullIf
//
//	@doc:
// 		Create a scalar nullif expr from a DXL scalar nullif
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarNullIf(const CDXLNode *pdxlnNullIf)
{
	GPOS_ASSERT(nullptr != pdxlnNullIf && 2 == pdxlnNullIf->Arity());
	CDXLScalarNullIf *dxl_op =
		CDXLScalarNullIf::Cast(pdxlnNullIf->GetOperator());

	// translate children
	CExpression *pexprLeft = Pexpr((*pdxlnNullIf)[0]);
	CExpression *pexprRight = Pexpr((*pdxlnNullIf)[1]);

	IMDId *mdid_op = dxl_op->MdIdOp();
	mdid_op->AddRef();

	IMDId *mdid_type = dxl_op->MdidType();
	mdid_type->AddRef();

	return GPOS_NEW(m_mp) CExpression(
		m_mp, GPOS_NEW(m_mp) CScalarNullIf(m_mp, mdid_op, mdid_type), pexprLeft,
		pexprRight);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarCmp
//
//	@doc:
// 		Create a scalar compare expr from a DXL scalar compare
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarCmp(const CDXLNode *pdxlnCmp)
{
	GPOS_ASSERT(nullptr != pdxlnCmp && 2 == pdxlnCmp->Arity());
	CDXLScalarComp *pdxlopComp = CDXLScalarComp::Cast(pdxlnCmp->GetOperator());
	// get children
	CDXLNode *dxlnode_left = (*pdxlnCmp)[0];
	CDXLNode *dxlnode_right = (*pdxlnCmp)[1];

	// translate left and right children
	CExpression *pexprLeft = Pexpr(dxlnode_left);
	CExpression *pexprRight = Pexpr(dxlnode_right);

	IMDId *mdid = pdxlopComp->MDId();
	mdid->AddRef();

	CScalarCmp *popScCmp = GPOS_NEW(m_mp) CScalarCmp(
		m_mp, mdid,
		GPOS_NEW(m_mp)
			CWStringConst(m_mp, pdxlopComp->GetComparisonOpName()->GetBuffer()),
		CUtils::ParseCmpType(mdid));

	GPOS_ASSERT(nullptr != popScCmp);
	GPOS_ASSERT(nullptr != popScCmp->Pstr());
	GPOS_ASSERT(nullptr != popScCmp->Pstr()->GetBuffer());

	CExpression *pexpr =
		GPOS_NEW(m_mp) CExpression(m_mp, popScCmp, pexprLeft, pexprRight);

	return pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarFunc
//
//	@doc:
// 		Create a scalar func operator expression from a DXL func expr
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarFunc(const CDXLNode *pdxlnFunc)
{
	GPOS_ASSERT(nullptr != pdxlnFunc);

	const ULONG length = pdxlnFunc->Arity();

	CDXLScalarFuncExpr *pdxlopFuncExpr =
		CDXLScalarFuncExpr::Cast(pdxlnFunc->GetOperator());

	COperator *pop = nullptr;

	IMDId *mdid_func = pdxlopFuncExpr->FuncMdId();
	mdid_func->AddRef();
	const IMDFunction *pmdfunc = m_pmda->RetrieveFunc(mdid_func);

	IMDId *mdid_return_type = pdxlopFuncExpr->ReturnTypeMdId();
	mdid_return_type->AddRef();

	CExpressionArray *pdrgpexprArgs = nullptr;
	IMDId *pmdidInput = nullptr;
	if (0 < length)
	{
		// translate function arguments
		pdrgpexprArgs = PdrgpexprChildren(pdxlnFunc);

		if (1 == length)
		{
			CExpression *pexprFirstChild = (*pdrgpexprArgs)[0];
			COperator *popFirstChild = pexprFirstChild->Pop();
			if (popFirstChild->FScalar())
			{
				pmdidInput = CScalar::PopConvert(popFirstChild)->MdidType();
			}
		}
	}

	if (CTranslatorDXLToExprUtils::FCastFunc(m_pmda, pdxlnFunc, pmdidInput))
	{
		const IMDCast *pmdcast = m_pmda->Pmdcast(pmdidInput, mdid_return_type);

		if (pmdcast->GetMDPathType() == IMDCast::EmdtArrayCoerce)
		{
			CMDArrayCoerceCastGPDB *parrayCoerceCast =
				(CMDArrayCoerceCastGPDB *) pmdcast;
			pop = GPOS_NEW(m_mp) CScalarArrayCoerceExpr(
				m_mp, parrayCoerceCast->GetCastFuncMdId(), mdid_return_type,
				parrayCoerceCast->TypeModifier(),
				parrayCoerceCast->IsExplicit(),
				(COperator::ECoercionForm) parrayCoerceCast->GetCoercionForm(),
				parrayCoerceCast->Location());
		}
		else
		{
			pop = GPOS_NEW(m_mp) CScalarCast(m_mp, mdid_return_type, mdid_func,
											 pmdcast->IsBinaryCoercible());
		}
	}
	else
	{
		pop = GPOS_NEW(m_mp) CScalarFunc(
			m_mp, mdid_func, mdid_return_type, pdxlopFuncExpr->TypeModifier(),
			GPOS_NEW(m_mp) CWStringConst(
				m_mp, (pmdfunc->Mdname().GetMDName())->GetBuffer()));
	}

	CExpression *pexprFunc = nullptr;
	if (nullptr != pdrgpexprArgs)
	{
		pexprFunc = GPOS_NEW(m_mp) CExpression(m_mp, pop, pdrgpexprArgs);
	}
	else
	{
		pexprFunc = GPOS_NEW(m_mp) CExpression(m_mp, pop);
	}

	if (IMDFunction::EfsVolatile == pmdfunc->GetFuncStability())
	{
		COptCtxt::PoctxtFromTLS()->SetHasVolatileFunc();
	}

	return pexprFunc;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprWindowFunc
//
//	@doc:
// 		Create a scalar window function expression from a DXL window ref
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprWindowFunc(const CDXLNode *pdxlnWindowRef)
{
	CDXLScalarWindowRef *pdxlopWinref =
		CDXLScalarWindowRef::Cast(pdxlnWindowRef->GetOperator());

	IMDId *mdid_func = pdxlopWinref->FuncMdId();
	mdid_func->AddRef();

	CWStringConst *str_name = GPOS_NEW(m_mp) CWStringConst(
		m_mp,
		CMDAccessorUtils::PstrWindowFuncName(m_pmda, mdid_func)->GetBuffer());

	CScalarWindowFunc::EWinStage ews = Ews(pdxlopWinref->GetDxlWinStage());

	IMDId *mdid_return_type = pdxlopWinref->ReturnTypeMdId();
	mdid_return_type->AddRef();

	GPOS_ASSERT(nullptr != str_name);
	CScalarWindowFunc *popWindowFunc = GPOS_NEW(m_mp)
		CScalarWindowFunc(m_mp, mdid_func, mdid_return_type, str_name, ews,
						  pdxlopWinref->IsDistinct(), pdxlopWinref->IsStarArg(),
						  pdxlopWinref->IsSimpleAgg());

	CExpression *pexprWindowFunc = nullptr;
	if (0 < pdxlnWindowRef->Arity())
	{
		CExpressionArray *pdrgpexprArgs = PdrgpexprChildren(pdxlnWindowRef);

		pexprWindowFunc =
			GPOS_NEW(m_mp) CExpression(m_mp, popWindowFunc, pdrgpexprArgs);
	}
	else
	{
		// window function has no arguments
		pexprWindowFunc = GPOS_NEW(m_mp) CExpression(m_mp, popWindowFunc);
	}

	return pexprWindowFunc;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::Ews
//
//	@doc:
//		Translate the DXL representation of the window stage
//
//---------------------------------------------------------------------------
CScalarWindowFunc::EWinStage
CTranslatorDXLToExpr::Ews(EdxlWinStage edxlws)
{
	ULONG window_frame_boundary_to_frame_boundary_mapping[][2] = {
		{EdxlwinstageImmediate, CScalarWindowFunc::EwsImmediate},
		{EdxlwinstagePreliminary, CScalarWindowFunc::EwsPreliminary},
		{EdxlwinstageRowKey, CScalarWindowFunc::EwsRowKey}};
#ifdef GPOS_DEBUG
	const ULONG arity =
		GPOS_ARRAY_SIZE(window_frame_boundary_to_frame_boundary_mapping);
	GPOS_ASSERT(arity > (ULONG) edxlws && "Invalid window stage");
#endif
	CScalarWindowFunc::EWinStage ews = (CScalarWindowFunc::EWinStage)
		window_frame_boundary_to_frame_boundary_mapping[(ULONG) edxlws][1];

	return ews;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarCoalesce
//
//	@doc:
// 		Create a scalar coalesce expression from a DXL scalar coalesce
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarCoalesce(const CDXLNode *pdxlnCoalesce)
{
	GPOS_ASSERT(nullptr != pdxlnCoalesce);
	GPOS_ASSERT(0 < pdxlnCoalesce->Arity());

	CDXLScalarCoalesce *dxl_op =
		CDXLScalarCoalesce::Cast(pdxlnCoalesce->GetOperator());

	CExpressionArray *pdrgpexprChildren = PdrgpexprChildren(pdxlnCoalesce);

	IMDId *mdid = dxl_op->MdidType();
	mdid->AddRef();

	return GPOS_NEW(m_mp) CExpression(
		m_mp, GPOS_NEW(m_mp) CScalarCoalesce(m_mp, mdid), pdrgpexprChildren);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarMinMax
//
//	@doc:
// 		Create a scalar MinMax expression from a DXL scalar MinMax
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarMinMax(const CDXLNode *pdxlnMinMax)
{
	GPOS_ASSERT(nullptr != pdxlnMinMax);
	GPOS_ASSERT(0 < pdxlnMinMax->Arity());

	CDXLScalarMinMax *dxl_op =
		CDXLScalarMinMax::Cast(pdxlnMinMax->GetOperator());

	CExpressionArray *pdrgpexprChildren = PdrgpexprChildren(pdxlnMinMax);

	CDXLScalarMinMax::EdxlMinMaxType min_max_type = dxl_op->GetMinMaxType();
	GPOS_ASSERT(CDXLScalarMinMax::EmmtMin == min_max_type ||
				CDXLScalarMinMax::EmmtMax == min_max_type);

	CScalarMinMax::EScalarMinMaxType esmmt = CScalarMinMax::EsmmtMin;
	if (CDXLScalarMinMax::EmmtMax == min_max_type)
	{
		esmmt = CScalarMinMax::EsmmtMax;
	}

	IMDId *mdid = dxl_op->MdidType();
	mdid->AddRef();

	return GPOS_NEW(m_mp)
		CExpression(m_mp, GPOS_NEW(m_mp) CScalarMinMax(m_mp, mdid, esmmt),
					pdrgpexprChildren);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprAggFunc
//
//	@doc:
// 		Create a scalar agg func operator expression from a DXL aggref node
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprAggFunc(const CDXLNode *pdxlnAggref)
{
	CDXLScalarAggref *dxl_op =
		CDXLScalarAggref::Cast(pdxlnAggref->GetOperator());

	IMDId *agg_func_mdid = dxl_op->GetDXLAggFuncMDid();
	agg_func_mdid->AddRef();
	const IMDAggregate *pmdagg = m_pmda->RetrieveAgg(agg_func_mdid);

	EAggfuncStage agg_func_stage = EaggfuncstageLocal;
	if (EdxlaggstagePartial != dxl_op->GetDXLAggStage())
	{
		agg_func_stage = EaggfuncstageGlobal;
	}
	BOOL fSplit = (EdxlaggstageNormal != dxl_op->GetDXLAggStage());

	EAggfuncKind agg_func_kind = EaggfunckindNormal;
	switch (dxl_op->GetAggKind())
	{
		case EdxlaggkindNormal:
		{
			agg_func_kind = EaggfunckindNormal;
			break;
		}
		case EdxlaggkindOrderedSet:
		{
			agg_func_kind = EaggfunckindOrderedSet;
			break;
		}
		case EdxlaggkindHypothetical:
		{
			agg_func_kind = EaggfunckindHypothetical;
			break;
		}
	}

	IMDId *resolved_return_type_mdid = dxl_op->GetDXLResolvedRetTypeMDid();
	if (nullptr != resolved_return_type_mdid)
	{
		// use the resolved type provided in DXL
		resolved_return_type_mdid->AddRef();
	}

	dxl_op->GetArgTypes()->AddRef();
	CScalarAggFunc *popScAggFunc = CUtils::PopAggFunc(
		m_mp, agg_func_mdid,
		GPOS_NEW(m_mp)
			CWStringConst(m_mp, (pmdagg->Mdname().GetMDName())->GetBuffer()),
		dxl_op->IsDistinct(), agg_func_stage, fSplit, resolved_return_type_mdid,
		agg_func_kind, dxl_op->GetArgTypes(), pmdagg->IsAggRepSafe());

	CExpression *pexprAggFunc = nullptr;

	if (0 < pdxlnAggref->Arity())
	{
		// translate function arguments
		CExpressionArray *pdrgpexprArgs = PdrgpexprChildren(pdxlnAggref);

		// check if the arguments have set returning functions, if so raise an exception
		for (ULONG ul = 0; ul < (*pdrgpexprArgs)[0]->Arity(); ul++)
		{
			CExpression *pexprAggrefChild = (*(*pdrgpexprArgs)[0])[ul];

			if (pexprAggrefChild->DeriveHasNonScalarFunction())
			{
				GPOS_RAISE(
					gpopt::ExmaGPOPT, gpopt::ExmiUnsupportedOp,
					GPOS_WSZ_LIT(
						"Aggregate function with set returning attributes"));
			}
		}

		pexprAggFunc =
			GPOS_NEW(m_mp) CExpression(m_mp, popScAggFunc, pdrgpexprArgs);
	}
	else
	{
		// aggregate function has no arguments
		pexprAggFunc = GPOS_NEW(m_mp) CExpression(m_mp, popScAggFunc);
	}

	return pexprAggFunc;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprArray
//
//	@doc:
// 		Translate a scalar array
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprArray(const CDXLNode *dxlnode)
{
	CDXLScalarArray *dxl_op = CDXLScalarArray::Cast(dxlnode->GetOperator());

	IMDId *elem_type_mdid = dxl_op->ElementTypeMDid();
	elem_type_mdid->AddRef();

	IMDId *array_type_mdid = dxl_op->ArrayTypeMDid();
	array_type_mdid->AddRef();

	CScalarArray *popArray = GPOS_NEW(m_mp) CScalarArray(
		m_mp, elem_type_mdid, array_type_mdid, dxl_op->IsMultiDimensional());

	CExpressionArray *pdrgpexprChildren = PdrgpexprChildren(dxlnode);

	CExpression *pexprArray =
		GPOS_NEW(m_mp) CExpression(m_mp, popArray, pdrgpexprChildren);

	CExpression *pexprCollapsedArray =
		CUtils::PexprCollapseConstArray(m_mp, pexprArray);

	pexprArray->Release();

	return pexprCollapsedArray;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprArrayRef
//
//	@doc:
// 		Translate a scalar arrayref
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprArrayRef(const CDXLNode *dxlnode)
{
	CDXLScalarArrayRef *dxl_op =
		CDXLScalarArrayRef::Cast(dxlnode->GetOperator());

	IMDId *elem_type_mdid = dxl_op->ElementTypeMDid();
	elem_type_mdid->AddRef();

	IMDId *array_type_mdid = dxl_op->ArrayTypeMDid();
	array_type_mdid->AddRef();

	IMDId *return_type_mdid = dxl_op->ReturnTypeMDid();
	return_type_mdid->AddRef();

	CScalarArrayRef *popArrayref = GPOS_NEW(m_mp)
		CScalarArrayRef(m_mp, elem_type_mdid, dxl_op->TypeModifier(),
						array_type_mdid, return_type_mdid);

	CExpressionArray *pdrgpexprChildren = PdrgpexprChildren(dxlnode);

	return GPOS_NEW(m_mp) CExpression(m_mp, popArrayref, pdrgpexprChildren);
}

CExpression *
CTranslatorDXLToExpr::PexprValuesList(const CDXLNode *dxlnode)
{
	CScalarValuesList *popScalarValuesList =
		GPOS_NEW(m_mp) CScalarValuesList(m_mp);

	CExpressionArray *pdrgpexprChildren = PdrgpexprChildren(dxlnode);

	return GPOS_NEW(m_mp)
		CExpression(m_mp, popScalarValuesList, pdrgpexprChildren);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprArrayRefIndexList
//
//	@doc:
// 		Translate a scalar arrayref index list
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprArrayRefIndexList(const CDXLNode *dxlnode)
{
	CDXLScalarArrayRefIndexList *dxl_op =
		CDXLScalarArrayRefIndexList::Cast(dxlnode->GetOperator());
	CScalarArrayRefIndexList *popIndexlist = GPOS_NEW(m_mp)
		CScalarArrayRefIndexList(m_mp, Eilt(dxl_op->GetDXLIndexListBound()));

	CExpressionArray *pdrgpexprChildren = PdrgpexprChildren(dxlnode);

	return GPOS_NEW(m_mp) CExpression(m_mp, popIndexlist, pdrgpexprChildren);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::Eilt
//
//	@doc:
// 		Translate the arrayref index list type
//
//---------------------------------------------------------------------------
CScalarArrayRefIndexList::EIndexListType
CTranslatorDXLToExpr::Eilt(
	const CDXLScalarArrayRefIndexList::EIndexListBound eilb)
{
	switch (eilb)
	{
		case CDXLScalarArrayRefIndexList::EilbLower:
			return CScalarArrayRefIndexList::EiltLower;

		case CDXLScalarArrayRefIndexList::EilbUpper:
			return CScalarArrayRefIndexList::EiltUpper;

		default:
			GPOS_RAISE(gpopt::ExmaGPOPT, gpopt::ExmiUnsupportedOp,
					   GPOS_WSZ_LIT("Invalid arrayref index type"));
			return CScalarArrayRefIndexList::EiltSentinel;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprArrayCmp
//
//	@doc:
// 		Translate a scalar array compare
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprArrayCmp(const CDXLNode *dxlnode)
{
	CDXLScalarArrayComp *dxl_op =
		CDXLScalarArrayComp::Cast(dxlnode->GetOperator());

	IMDId *mdid_op = dxl_op->MDId();
	mdid_op->AddRef();

	const CWStringConst *str_opname = dxl_op->GetComparisonOpName();

	EdxlArrayCompType edxlarrcmp = dxl_op->GetDXLArrayCmpType();
	CScalarArrayCmp::EArrCmpType earrcmpt = CScalarArrayCmp::EarrcmpSentinel;
	if (Edxlarraycomptypeall == edxlarrcmp)
	{
		earrcmpt = CScalarArrayCmp::EarrcmpAll;
	}
	else
	{
		GPOS_ASSERT(Edxlarraycomptypeany == edxlarrcmp);
		earrcmpt = CScalarArrayCmp::EarrcmpAny;
	}

	CScalarArrayCmp *popArrayCmp = GPOS_NEW(m_mp) CScalarArrayCmp(
		m_mp, mdid_op,
		GPOS_NEW(m_mp) CWStringConst(m_mp, str_opname->GetBuffer()), earrcmpt);

	CExpressionArray *pdrgpexprChildren = PdrgpexprChildren(dxlnode);

	return GPOS_NEW(m_mp) CExpression(m_mp, popArrayCmp, pdrgpexprChildren);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarIdent
//
//	@doc:
// 		Create a scalar ident expr from a DXL scalar ident
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarIdent(const CDXLNode *pdxlnIdent)
{
	// get dxl scalar identifier
	CDXLScalarIdent *dxl_op = CDXLScalarIdent::Cast(pdxlnIdent->GetOperator());

	// get the dxl column reference
	const CDXLColRef *dxl_colref = dxl_op->GetDXLColRef();
	const ULONG colid = dxl_colref->Id();

	// get its column reference from the hash map
	const CColRef *colref = LookupColRef(m_phmulcr, colid);
	CExpression *pexpr = GPOS_NEW(m_mp)
		CExpression(m_mp, GPOS_NEW(m_mp) CScalarIdent(m_mp, colref));

	return pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PdrgpexprChildren
//
//	@doc:
// 		Translate children of a DXL node
//
//---------------------------------------------------------------------------
CExpressionArray *
CTranslatorDXLToExpr::PdrgpexprChildren(const CDXLNode *dxlnode)
{
	CExpressionArray *pdrgpexpr = GPOS_NEW(m_mp) CExpressionArray(m_mp);

	const ULONG arity = dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		// get next child and translate it
		CDXLNode *child_dxlnode = (*dxlnode)[ul];

		CExpression *pexprChild = Pexpr(child_dxlnode);
		pdrgpexpr->Append(pexprChild);
	}

	return pdrgpexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarIf
//
//	@doc:
// 		Create a scalar if expression from a DXL scalar if statement
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarIf(const CDXLNode *pdxlnIfStmt)
{
	GPOS_ASSERT(nullptr != pdxlnIfStmt);
	GPOS_ASSERT(3 == pdxlnIfStmt->Arity());

	CDXLScalarIfStmt *dxl_op =
		CDXLScalarIfStmt::Cast(pdxlnIfStmt->GetOperator());

	CExpressionArray *pdrgpexprChildren = PdrgpexprChildren(pdxlnIfStmt);

	IMDId *mdid = dxl_op->GetResultTypeMdId();
	mdid->AddRef();
	CScalarIf *popScIf = GPOS_NEW(m_mp) CScalarIf(m_mp, mdid);
	CExpression *pexpr =
		GPOS_NEW(m_mp) CExpression(m_mp, popScIf, pdrgpexprChildren);

	return pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarSwitch
//
//	@doc:
// 		Create a scalar switch expression from a DXL scalar switch
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarSwitch(const CDXLNode *pdxlnSwitch)
{
	GPOS_ASSERT(nullptr != pdxlnSwitch);
	GPOS_ASSERT(1 < pdxlnSwitch->Arity());

	CDXLScalarSwitch *dxl_op =
		CDXLScalarSwitch::Cast(pdxlnSwitch->GetOperator());

	CExpressionArray *pdrgpexprChildren = PdrgpexprChildren(pdxlnSwitch);

	IMDId *mdid = dxl_op->MdidType();
	mdid->AddRef();
	CScalarSwitch *pop = GPOS_NEW(m_mp) CScalarSwitch(m_mp, mdid);

	return GPOS_NEW(m_mp) CExpression(m_mp, pop, pdrgpexprChildren);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarSwitchCase
//
//	@doc:
// 		Create a scalar switchcase expression from a DXL scalar switchcase
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarSwitchCase(const CDXLNode *pdxlnSwitchCase)
{
	GPOS_ASSERT(nullptr != pdxlnSwitchCase);

	GPOS_ASSERT(2 == pdxlnSwitchCase->Arity());

	CExpressionArray *pdrgpexprChildren = PdrgpexprChildren(pdxlnSwitchCase);

	return GPOS_NEW(m_mp) CExpression(
		m_mp, GPOS_NEW(m_mp) CScalarSwitchCase(m_mp), pdrgpexprChildren);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarCaseTest
//
//	@doc:
// 		Create a scalar case test expression from a DXL scalar case test
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarCaseTest(const CDXLNode *pdxlnCaseTest)
{
	GPOS_ASSERT(nullptr != pdxlnCaseTest);

	CDXLScalarCaseTest *dxl_op =
		CDXLScalarCaseTest::Cast(pdxlnCaseTest->GetOperator());

	IMDId *mdid = dxl_op->MdidType();
	mdid->AddRef();
	CScalarCaseTest *pop = GPOS_NEW(m_mp) CScalarCaseTest(m_mp, mdid);

	return GPOS_NEW(m_mp) CExpression(m_mp, pop);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarNullTest
//
//	@doc:
// 		Create a scalar null test expr from a DXL scalar null test
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarNullTest(const CDXLNode *pdxlnNullTest)
{
	// get dxl scalar null test
	CDXLScalarNullTest *dxl_op =
		CDXLScalarNullTest::Cast(pdxlnNullTest->GetOperator());

	GPOS_ASSERT(nullptr != dxl_op);

	// translate child expression
	GPOS_ASSERT(1 == pdxlnNullTest->Arity());

	CDXLNode *child_dxlnode = (*pdxlnNullTest)[0];
	CExpression *pexprChild = Pexpr(child_dxlnode);

	CExpression *pexpr = GPOS_NEW(m_mp)
		CExpression(m_mp, GPOS_NEW(m_mp) CScalarNullTest(m_mp), pexprChild);

	if (!dxl_op->IsNullTest())
	{
		// IS NOT NULL test: add a not expression on top
		pexpr = GPOS_NEW(m_mp) CExpression(
			m_mp, GPOS_NEW(m_mp) CScalarBoolOp(m_mp, CScalarBoolOp::EboolopNot),
			pexpr);
	}

	return pexpr;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarBooleanTest
//
//	@doc:
// 		Create a scalar boolean test expr from a DXL scalar boolean test
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarBooleanTest(const CDXLNode *pdxlnScBoolTest)
{
	const ULONG rgulBoolTestMapping[][2] = {
		{EdxlbooleantestIsTrue, CScalarBooleanTest::EbtIsTrue},
		{EdxlbooleantestIsNotTrue, CScalarBooleanTest::EbtIsNotTrue},
		{EdxlbooleantestIsFalse, CScalarBooleanTest::EbtIsFalse},
		{EdxlbooleantestIsNotFalse, CScalarBooleanTest::EbtIsNotFalse},
		{EdxlbooleantestIsUnknown, CScalarBooleanTest::EbtIsUnknown},
		{EdxlbooleantestIsNotUnknown, CScalarBooleanTest::EbtIsNotUnknown},
	};

	// get dxl scalar null test
	CDXLScalarBooleanTest *dxl_op =
		CDXLScalarBooleanTest::Cast(pdxlnScBoolTest->GetOperator());

	GPOS_ASSERT(nullptr != dxl_op);

	// translate child expression
	GPOS_ASSERT(1 == pdxlnScBoolTest->Arity());

	CDXLNode *child_dxlnode = (*pdxlnScBoolTest)[0];
	CExpression *pexprChild = Pexpr(child_dxlnode);

	CScalarBooleanTest::EBoolTest ebt = (CScalarBooleanTest::EBoolTest)(
		rgulBoolTestMapping[dxl_op->GetDxlBoolTypeStr()][1]);

	return GPOS_NEW(m_mp) CExpression(
		m_mp, GPOS_NEW(m_mp) CScalarBooleanTest(m_mp, ebt), pexprChild);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarCast
//
//	@doc:
// 		Create a scalar relabel type from a DXL scalar cast
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarCast(const CDXLNode *pdxlnCast)
{
	// get dxl scalar relabel type
	CDXLScalarCast *dxl_op = CDXLScalarCast::Cast(pdxlnCast->GetOperator());
	GPOS_ASSERT(nullptr != dxl_op);

	// translate child expression
	GPOS_ASSERT(1 == pdxlnCast->Arity());
	CDXLNode *child_dxlnode = (*pdxlnCast)[0];
	CExpression *pexprChild = Pexpr(child_dxlnode);

	IMDId *mdid_type = dxl_op->MdidType();
	IMDId *mdid_func = dxl_op->FuncMdId();
	mdid_type->AddRef();
	mdid_func->AddRef();

	COperator *popChild = pexprChild->Pop();
	IMDId *pmdidInput = CScalar::PopConvert(popChild)->MdidType();
	const IMDCast *pmdcast = m_pmda->Pmdcast(pmdidInput, mdid_type);
	BOOL fRelabel = pmdcast->IsBinaryCoercible();

	CExpression *pexpr;

	if (pmdcast->GetMDPathType() == IMDCast::EmdtArrayCoerce)
	{
		CMDArrayCoerceCastGPDB *parrayCoerceCast =
			(CMDArrayCoerceCastGPDB *) pmdcast;
		pexpr = GPOS_NEW(m_mp) CExpression(
			m_mp,
			GPOS_NEW(m_mp) CScalarArrayCoerceExpr(
				m_mp, parrayCoerceCast->GetCastFuncMdId(), mdid_type,
				parrayCoerceCast->TypeModifier(),
				parrayCoerceCast->IsExplicit(),
				(COperator::ECoercionForm) parrayCoerceCast->GetCoercionForm(),
				parrayCoerceCast->Location()),
			pexprChild);
	}
	else
	{
		pexpr = GPOS_NEW(m_mp) CExpression(
			m_mp,
			GPOS_NEW(m_mp) CScalarCast(m_mp, mdid_type, mdid_func, fRelabel),
			pexprChild);
	}

	return pexpr;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarCoerceToDomain
//
//	@doc:
// 		Create a scalar CoerceToDomain from a DXL scalar CoerceToDomain
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarCoerceToDomain(const CDXLNode *pdxlnCoerce)
{
	// get dxl scalar coerce operator
	CDXLScalarCoerceToDomain *dxl_op =
		CDXLScalarCoerceToDomain::Cast(pdxlnCoerce->GetOperator());
	GPOS_ASSERT(nullptr != dxl_op);

	// translate child expression
	GPOS_ASSERT(1 == pdxlnCoerce->Arity());
	CDXLNode *child_dxlnode = (*pdxlnCoerce)[0];
	CExpression *pexprChild = Pexpr(child_dxlnode);

	IMDId *mdid_type = dxl_op->GetResultTypeMdId();
	mdid_type->AddRef();

	EdxlCoercionForm dxl_coerce_format = dxl_op->GetDXLCoercionForm();

	return GPOS_NEW(m_mp) CExpression(
		m_mp,
		GPOS_NEW(m_mp) CScalarCoerceToDomain(
			m_mp, mdid_type, dxl_op->TypeModifier(),
			(COperator::ECoercionForm)
				dxl_coerce_format,	// map Coercion Form directly based on position in enum
			dxl_op->GetLocation()),
		pexprChild);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarCoerceViaIO
//
//	@doc:
// 		Create a scalar CoerceViaIO from a DXL scalar CoerceViaIO
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarCoerceViaIO(const CDXLNode *pdxlnCoerce)
{
	// get dxl scalar coerce operator
	CDXLScalarCoerceViaIO *dxl_op =
		CDXLScalarCoerceViaIO::Cast(pdxlnCoerce->GetOperator());
	GPOS_ASSERT(nullptr != dxl_op);

	// translate child expression
	GPOS_ASSERT(1 == pdxlnCoerce->Arity());
	CDXLNode *child_dxlnode = (*pdxlnCoerce)[0];
	CExpression *pexprChild = Pexpr(child_dxlnode);

	IMDId *mdid_type = dxl_op->GetResultTypeMdId();
	mdid_type->AddRef();

	EdxlCoercionForm dxl_coerce_format = dxl_op->GetDXLCoercionForm();

	return GPOS_NEW(m_mp) CExpression(
		m_mp,
		GPOS_NEW(m_mp) CScalarCoerceViaIO(
			m_mp, mdid_type, dxl_op->TypeModifier(),
			(COperator::ECoercionForm)
				dxl_coerce_format,	// map Coercion Form directly based on position in enum
			dxl_op->GetLocation()),
		pexprChild);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarArrayCoerceExpr
//
//	@doc:
// 		Create a scalar array coerce expr from a DXL scalar array coerce expr
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarArrayCoerceExpr(
	const CDXLNode *pdxlnArrayCoerceExpr)
{
	GPOS_ASSERT(nullptr != pdxlnArrayCoerceExpr);

	CDXLScalarArrayCoerceExpr *dxl_op =
		CDXLScalarArrayCoerceExpr::Cast(pdxlnArrayCoerceExpr->GetOperator());

	GPOS_ASSERT(1 == pdxlnArrayCoerceExpr->Arity());
	CDXLNode *child_dxlnode = (*pdxlnArrayCoerceExpr)[0];
	CExpression *pexprChild = Pexpr(child_dxlnode);

	IMDId *element_func = dxl_op->GetCoerceFuncMDid();
	element_func->AddRef();

	IMDId *result_type_mdid = dxl_op->GetResultTypeMdId();
	result_type_mdid->AddRef();

	EdxlCoercionForm dxl_coerce_format = dxl_op->GetDXLCoercionForm();

	return GPOS_NEW(m_mp) CExpression(
		m_mp,
		GPOS_NEW(m_mp) CScalarArrayCoerceExpr(
			m_mp, element_func, result_type_mdid, dxl_op->TypeModifier(),
			dxl_op->IsExplicit(),
			(COperator::ECoercionForm)
				dxl_coerce_format,	// map Coercion Form directly based on position in enum
			dxl_op->GetLocation()),
		pexprChild);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarConst
//
//	@doc:
// 		Create a scalar const expr from a DXL scalar constant value
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarConst(const CDXLNode *pdxlnConstVal)
{
	GPOS_ASSERT(nullptr != pdxlnConstVal);

	// translate the dxl scalar const value
	CDXLScalarConstValue *dxl_op =
		CDXLScalarConstValue::Cast(pdxlnConstVal->GetOperator());
	CScalarConst *popConst =
		CTranslatorDXLToExprUtils::PopConst(m_mp, m_pmda, dxl_op);

	return GPOS_NEW(m_mp) CExpression(m_mp, popConst);
}

CExpression *
CTranslatorDXLToExpr::PexprSortGroupClause(const CDXLNode *pdxlnSortGroupClause)
{
	GPOS_ASSERT(nullptr != pdxlnSortGroupClause);

	// translate the dxl scalar const value
	CDXLScalarSortGroupClause *dxl_op =
		CDXLScalarSortGroupClause::Cast(pdxlnSortGroupClause->GetOperator());
	CScalarSortGroupClause *sgc = GPOS_NEW(m_mp) CScalarSortGroupClause(
		m_mp, dxl_op->Index(), dxl_op->EqOp(), dxl_op->SortOp(),
		dxl_op->NullsFirst(), dxl_op->IsHashable());

	return GPOS_NEW(m_mp) CExpression(m_mp, sgc);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarSubquery
//
//	@doc:
// 		Create a scalar subquery expr from a DXL scalar subquery node
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarSubquery(const CDXLNode *pdxlnSubquery)
{
	GPOS_ASSERT(nullptr != pdxlnSubquery);
	CDXLScalarSubquery *pdxlopSubquery =
		CDXLScalarSubquery::Cast(pdxlnSubquery->GetOperator());

	// translate child
	CDXLNode *child_dxlnode = (*pdxlnSubquery)[0];
	CExpression *pexprChild = Pexpr(child_dxlnode);

	// get subquery colref for colid
	ULONG colid = pdxlopSubquery->GetColId();
	const CColRef *colref = LookupColRef(m_phmulcr, colid);

	CScalarSubquery *popScalarSubquery = GPOS_NEW(m_mp)
		CScalarSubquery(m_mp, colref, false /*fGeneratedByExist*/,
						false /*fGeneratedByQuantified*/);
	GPOS_ASSERT(nullptr != popScalarSubquery);
	CExpression *pexpr =
		GPOS_NEW(m_mp) CExpression(m_mp, popScalarSubquery, pexprChild);

	return pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarProjElem
//
//	@doc:
// 		Create a scalar project elem expression from a DXL scalar project elem node
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarProjElem(const CDXLNode *pdxlnPrEl)
{
	GPOS_ASSERT(nullptr != pdxlnPrEl &&
				EdxlopScalarProjectElem ==
					pdxlnPrEl->GetOperator()->GetDXLOperator());
	GPOS_ASSERT(1 == pdxlnPrEl->Arity());

	CDXLScalarProjElem *pdxlopPrEl =
		CDXLScalarProjElem::Cast(pdxlnPrEl->GetOperator());

	// translate child
	CDXLNode *child_dxlnode = (*pdxlnPrEl)[0];
	CExpression *pexprChild = Pexpr(child_dxlnode);

	CScalar *popScalar = CScalar::PopConvert(pexprChild->Pop());

	IMDId *mdid = popScalar->MdidType();
	const IMDType *pmdtype = m_pmda->RetrieveType(mdid);

	CName name(pdxlopPrEl->GetMdNameAlias()->GetMDName());

	// generate a new column reference
	CColRef *colref =
		m_pcf->PcrCreate(pmdtype, popScalar->TypeModifier(), name);

	// store colid -> colref mapping
	BOOL fInserted GPOS_ASSERTS_ONLY =
		m_phmulcr->Insert(GPOS_NEW(m_mp) ULONG(pdxlopPrEl->Id()), colref);

	GPOS_ASSERT(fInserted);

	CExpression *pexprProjElem = GPOS_NEW(m_mp) CExpression(
		m_mp, GPOS_NEW(m_mp) CScalarProjectElement(m_mp, colref), pexprChild);
	return pexprProjElem;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::PexprScalarProjList
//
//	@doc:
// 		Create a scalar project list expression from a DXL scalar project list node
//
//---------------------------------------------------------------------------
CExpression *
CTranslatorDXLToExpr::PexprScalarProjList(const CDXLNode *pdxlnPrL)
{
	GPOS_ASSERT(nullptr != pdxlnPrL &&
				EdxlopScalarProjectList ==
					pdxlnPrL->GetOperator()->GetDXLOperator());

	// translate project elements
	CExpression *pexprProjList = nullptr;

	if (0 == pdxlnPrL->Arity())
	{
		pexprProjList = GPOS_NEW(m_mp)
			CExpression(m_mp, GPOS_NEW(m_mp) CScalarProjectList(m_mp));
	}
	else
	{
		CExpressionArray *pdrgpexprProjElems =
			GPOS_NEW(m_mp) CExpressionArray(m_mp);

		const ULONG length = pdxlnPrL->Arity();
		for (ULONG ul = 0; ul < length; ul++)
		{
			CDXLNode *pdxlnProjElem = (*pdxlnPrL)[ul];
			CExpression *pexprProjElem = PexprScalarProjElem(pdxlnProjElem);
			pdrgpexprProjElems->Append(pexprProjElem);
		}

		pexprProjList = GPOS_NEW(m_mp) CExpression(
			m_mp, GPOS_NEW(m_mp) CScalarProjectList(m_mp), pdrgpexprProjElems);
	}

	return pexprProjList;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::Pos
//
//	@doc:
// 		Construct an order spec from a DXL sort col list node
//
//---------------------------------------------------------------------------
COrderSpec *
CTranslatorDXLToExpr::Pos(const CDXLNode *dxlnode)
{
	GPOS_ASSERT(nullptr != dxlnode);

	COrderSpec *pos = GPOS_NEW(m_mp) COrderSpec(m_mp);

	const ULONG length = dxlnode->Arity();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CDXLNode *pdxlnSortCol = (*dxlnode)[ul];

		CDXLScalarSortCol *dxl_op =
			CDXLScalarSortCol::Cast(pdxlnSortCol->GetOperator());
		const ULONG colid = dxl_op->GetColId();

		// get its column reference from the hash map
		CColRef *colref = LookupColRef(m_phmulcr, colid);

		IMDId *sort_op_id = dxl_op->GetMdIdSortOp();
		sort_op_id->AddRef();

		COrderSpec::ENullTreatment ent = COrderSpec::EntLast;
		if (dxl_op->IsSortedNullsFirst())
		{
			ent = COrderSpec::EntFirst;
		}

		pos->Append(sort_op_id, colref, ent);
	}

	return pos;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorDXLToExpr::AddDistributionColumns
//
//	@doc:
// 		Add distribution column info from the MD relation to the table descriptor
//
//---------------------------------------------------------------------------
void
CTranslatorDXLToExpr::AddDistributionColumns(
	CTableDescriptor *ptabdesc, const IMDRelation *pmdrel,
	IntToUlongMap *phmiulAttnoColMapping)
{
	GPOS_ASSERT(nullptr != ptabdesc);
	GPOS_ASSERT(nullptr != pmdrel);

	// compute distribution columns for table descriptor
	ULONG num_cols = pmdrel->DistrColumnCount();
	for (ULONG ul = 0; ul < num_cols; ul++)
	{
		const IMDColumn *pmdcol = pmdrel->GetDistrColAt(ul);
		INT attno = pmdcol->AttrNum();
		ULONG *pulPos = phmiulAttnoColMapping->Find(&attno);
		GPOS_ASSERT(nullptr != pulPos);

		IMDId *opfamily = nullptr;
		if (GPOS_FTRACE(EopttraceConsiderOpfamiliesForDistribution))
		{
			opfamily = pmdrel->GetDistrOpfamilyAt(ul);
			GPOS_ASSERT(nullptr != opfamily && opfamily->IsValid());
			//opfamily->AddRef();
		}

		ptabdesc->AddDistributionColumn(*pulPos, opfamily);
	}
}

void
CTranslatorDXLToExpr::MarkUnknownColsAsUnused()
{
	UlongToColRefMapIter iter(m_phmulcr);

	while (iter.Advance())
	{
		CColRef *colref = m_phmulcr->Find(iter.Key());
		if (colref->GetUsage() == CColRef::EUnknown)
		{
			colref->MarkAsUnused();
		}
	}
}

// EOF

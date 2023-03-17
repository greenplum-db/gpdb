//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CTranslatorExprToDXLUtils.cpp
//
//	@doc:
//		Implementation of the helper methods used during Expr to DXL translation
//
//---------------------------------------------------------------------------

#include "gpopt/translate/CTranslatorExprToDXLUtils.h"

#include "gpopt/base/CConstraint.h"
#include "gpopt/base/CConstraintConjunction.h"
#include "gpopt/base/CConstraintDisjunction.h"
#include "gpopt/base/CConstraintInterval.h"
#include "gpopt/base/CConstraintNegation.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/exception.h"
#include "gpopt/mdcache/CMDAccessorUtils.h"
#include "gpopt/operators/CPhysicalAgg.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/operators/CScalarCast.h"
#include "gpopt/operators/CScalarIdent.h"
#include "naucrates/dxl/operators/CDXLDatumBool.h"
#include "naucrates/dxl/operators/CDXLDatumInt4.h"
#include "naucrates/dxl/operators/CDXLDatumOid.h"
#include "naucrates/dxl/operators/CDXLDirectDispatchInfo.h"
#include "naucrates/dxl/operators/CDXLPhysicalHashJoin.h"
#include "naucrates/dxl/operators/CDXLPhysicalPartitionSelector.h"
#include "naucrates/dxl/operators/CDXLPhysicalResult.h"
#include "naucrates/dxl/operators/CDXLPhysicalValuesScan.h"
#include "naucrates/dxl/operators/CDXLScalarArrayCoerceExpr.h"
#include "naucrates/dxl/operators/CDXLScalarArrayComp.h"
#include "naucrates/dxl/operators/CDXLScalarCast.h"
#include "naucrates/dxl/operators/CDXLScalarIdent.h"
#include "naucrates/dxl/operators/CDXLScalarProjElem.h"
#include "naucrates/dxl/operators/CDXLScalarProjList.h"
#include "naucrates/dxl/operators/CDXLScalarValuesList.h"
#include "naucrates/exception.h"
#include "naucrates/md/IMDCast.h"
#include "naucrates/md/IMDScalarOp.h"
#include "naucrates/md/IMDTypeBool.h"
#include "naucrates/md/IMDTypeInt4.h"
#include "naucrates/md/IMDTypeOid.h"
#include "naucrates/statistics/IStatistics.h"

using namespace gpos;
using namespace gpmd;
using namespace gpdxl;
using namespace gpopt;
using namespace gpnaucrates;


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::PdxlpropCopy
//
//	@doc:
//		Return a copy the dxl node's physical properties
//
//---------------------------------------------------------------------------
CDXLPhysicalProperties *
CTranslatorExprToDXLUtils::PdxlpropCopy(CMemoryPool *mp, CDXLNode *dxlnode)
{
	GPOS_ASSERT(nullptr != dxlnode);

	GPOS_ASSERT(nullptr != dxlnode->GetProperties());
	CDXLPhysicalProperties *dxl_properties =
		CDXLPhysicalProperties::PdxlpropConvert(dxlnode->GetProperties());

	CWStringDynamic *pstrStartupcost = GPOS_NEW(mp) CWStringDynamic(
		mp,
		dxl_properties->GetDXLOperatorCost()->GetStartUpCostStr()->GetBuffer());
	CWStringDynamic *pstrCost = GPOS_NEW(mp) CWStringDynamic(
		mp,
		dxl_properties->GetDXLOperatorCost()->GetTotalCostStr()->GetBuffer());
	CWStringDynamic *rows_out_str = GPOS_NEW(mp) CWStringDynamic(
		mp, dxl_properties->GetDXLOperatorCost()->GetRowsOutStr()->GetBuffer());
	CWStringDynamic *width_str = GPOS_NEW(mp) CWStringDynamic(
		mp, dxl_properties->GetDXLOperatorCost()->GetWidthStr()->GetBuffer());

	return GPOS_NEW(mp) CDXLPhysicalProperties(GPOS_NEW(mp) CDXLOperatorCost(
		pstrStartupcost, pstrCost, rows_out_str, width_str));
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::PcrCreate
//
//	@doc:
//		Construct a column reference with the given name and type
//
//---------------------------------------------------------------------------
CColRef *
CTranslatorExprToDXLUtils::PcrCreate(CMemoryPool *mp, CMDAccessor *md_accessor,
									 CColumnFactory *col_factory,
									 IMDId *mdid_type, INT type_modifier,
									 const WCHAR *wszName)
{
	const IMDType *pmdtype = md_accessor->RetrieveType(mdid_type);

	CName *pname = GPOS_NEW(mp)
		CName(GPOS_NEW(mp) CWStringConst(wszName), true /*fOwnsMemory*/);
	CColRef *colref = col_factory->PcrCreate(pmdtype, type_modifier, *pname);
	GPOS_DELETE(pname);
	return colref;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::GetProperties
//
//	@doc:
//		Construct a DXL physical properties container with operator costs for
//		the given expression
//
//---------------------------------------------------------------------------
CDXLPhysicalProperties *
CTranslatorExprToDXLUtils::GetProperties(CMemoryPool *mp)
{
	// TODO:  - May 10, 2012; replace the dummy implementation with a real one
	CWStringDynamic *pstrStartupcost =
		GPOS_NEW(mp) CWStringDynamic(mp, GPOS_WSZ_LIT("10"));
	CWStringDynamic *pstrTotalcost =
		GPOS_NEW(mp) CWStringDynamic(mp, GPOS_WSZ_LIT("100"));
	CWStringDynamic *rows_out_str =
		GPOS_NEW(mp) CWStringDynamic(mp, GPOS_WSZ_LIT("100"));
	CWStringDynamic *width_str =
		GPOS_NEW(mp) CWStringDynamic(mp, GPOS_WSZ_LIT("4"));

	CDXLOperatorCost *cost = GPOS_NEW(mp) CDXLOperatorCost(
		pstrStartupcost, pstrTotalcost, rows_out_str, width_str);
	return GPOS_NEW(mp) CDXLPhysicalProperties(cost);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::FScalarConstTrue
//
//	@doc:
//		Checks to see if the DXL Node is a scalar const TRUE
//
//---------------------------------------------------------------------------
BOOL
CTranslatorExprToDXLUtils::FScalarConstTrue(CMDAccessor *md_accessor,
											CDXLNode *dxlnode)
{
	GPOS_ASSERT(nullptr != dxlnode);
	if (EdxlopScalarConstValue == dxlnode->GetOperator()->GetDXLOperator())
	{
		CDXLScalarConstValue *pdxlopConst =
			CDXLScalarConstValue::Cast(dxlnode->GetOperator());

		const IMDType *pmdtype =
			md_accessor->RetrieveType(pdxlopConst->GetDatumVal()->MDId());
		if (IMDType::EtiBool == pmdtype->GetDatumType())
		{
			CDXLDatumBool *dxl_datum = CDXLDatumBool::Cast(
				const_cast<CDXLDatum *>(pdxlopConst->GetDatumVal()));

			return (!dxl_datum->IsNull() && dxl_datum->GetValue());
		}
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::FScalarConstFalse
//
//	@doc:
//		Checks to see if the DXL Node is a scalar const FALSE
//
//---------------------------------------------------------------------------
BOOL
CTranslatorExprToDXLUtils::FScalarConstFalse(CMDAccessor *md_accessor,
											 CDXLNode *dxlnode)
{
	GPOS_ASSERT(nullptr != dxlnode);
	if (EdxlopScalarConstValue == dxlnode->GetOperator()->GetDXLOperator())
	{
		CDXLScalarConstValue *pdxlopConst =
			CDXLScalarConstValue::Cast(dxlnode->GetOperator());

		const IMDType *pmdtype =
			md_accessor->RetrieveType(pdxlopConst->GetDatumVal()->MDId());
		if (IMDType::EtiBool == pmdtype->GetDatumType())
		{
			CDXLDatumBool *dxl_datum = CDXLDatumBool::Cast(
				const_cast<CDXLDatum *>(pdxlopConst->GetDatumVal()));
			return (!dxl_datum->IsNull() && !dxl_datum->GetValue());
		}
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::PdxlnProjListFromChildProjList
//
//	@doc:
//		Construct a project list node by creating references to the columns
//		of the given project list of the child node
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXLUtils::PdxlnProjListFromChildProjList(
	CMemoryPool *mp, CColumnFactory *col_factory, ColRefToDXLNodeMap *phmcrdxln,
	const CDXLNode *pdxlnProjListChild)
{
	GPOS_ASSERT(nullptr != pdxlnProjListChild);

	CDXLScalarProjList *pdxlopPrL = GPOS_NEW(mp) CDXLScalarProjList(mp);
	CDXLNode *proj_list_dxlnode = GPOS_NEW(mp) CDXLNode(mp, pdxlopPrL);

	// create a scalar identifier for each project element of the child
	const ULONG arity = pdxlnProjListChild->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLNode *pdxlnProjElemChild = (*pdxlnProjListChild)[ul];

		// translate proj elem
		CDXLNode *pdxlnProjElem =
			PdxlnProjElem(mp, col_factory, phmcrdxln, pdxlnProjElemChild);
		proj_list_dxlnode->AddChild(pdxlnProjElem);
	}

	return proj_list_dxlnode;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::PdxlnProjElem
//
//	@doc:
//		Create a project elem as a scalar identifier for the given child
//		project element
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXLUtils::PdxlnProjElem(CMemoryPool *mp,
										 CColumnFactory *col_factory,
										 ColRefToDXLNodeMap *phmcrdxln,
										 const CDXLNode *pdxlnChildProjElem)
{
	GPOS_ASSERT(nullptr != pdxlnChildProjElem &&
				1 == pdxlnChildProjElem->Arity());

	CDXLScalarProjElem *pdxlopPrElChild =
		dynamic_cast<CDXLScalarProjElem *>(pdxlnChildProjElem->GetOperator());

	// find the col ref corresponding to this element's id through column factory
	CColRef *colref = col_factory->LookupColRef(pdxlopPrElChild->Id());
	if (nullptr == colref)
	{
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiExpr2DXLAttributeNotFound,
				   pdxlopPrElChild->Id());
	}

	CDXLNode *pdxlnProjElemResult = PdxlnProjElem(mp, phmcrdxln, colref);

	return pdxlnProjElemResult;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::ReplaceSubplan
//
//	@doc:
//		 Replace subplan entry in the given map with a dxl column ref
//
//---------------------------------------------------------------------------
void
CTranslatorExprToDXLUtils::ReplaceSubplan(
	CMemoryPool *mp,
	ColRefToDXLNodeMap *phmcrdxlnSubplans,	// map of col ref to subplan
	const CColRef *colref,					// key of entry in the passed map
	CDXLScalarProjElem *
		pdxlopPrEl	// project element to use for creating DXL col ref to replace subplan
)
{
	GPOS_ASSERT(nullptr != phmcrdxlnSubplans);
	GPOS_ASSERT(nullptr != colref);
	GPOS_ASSERT(pdxlopPrEl->Id() == colref->Id());

	IMDId *mdid_type = colref->RetrieveType()->MDId();
	mdid_type->AddRef();
	CMDName *mdname =
		GPOS_NEW(mp) CMDName(mp, pdxlopPrEl->GetMdNameAlias()->GetMDName());
	CDXLColRef *dxl_colref = GPOS_NEW(mp)
		CDXLColRef(mdname, pdxlopPrEl->Id(), mdid_type, colref->TypeModifier());
	CDXLScalarIdent *pdxlnScId = GPOS_NEW(mp) CDXLScalarIdent(mp, dxl_colref);
	CDXLNode *dxlnode = GPOS_NEW(mp) CDXLNode(mp, pdxlnScId);
	BOOL fReplaced GPOS_ASSERTS_ONLY =
		phmcrdxlnSubplans->Replace(colref, dxlnode);
	GPOS_ASSERT(fReplaced);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::PdxlnProjElem
//
//	@doc:
//		Create a project elem from a given col ref,
//
//		if the given col has a corresponding subplan entry in the given map,
//		the function returns a project element with a child subplan,
//		the function then replaces the subplan entry in the given map with the
//		projected column reference, so that all subplan references higher up in
//		the DXL tree use the projected col ref
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXLUtils::PdxlnProjElem(
	CMemoryPool *mp,
	ColRefToDXLNodeMap *
		phmcrdxlnSubplans,	// map of col ref -> subplan: can be modified by this function
	const CColRef *colref)
{
	GPOS_ASSERT(nullptr != colref);

	CMDName *mdname = GPOS_NEW(mp) CMDName(mp, colref->Name().Pstr());

	CDXLScalarProjElem *pdxlopPrEl =
		GPOS_NEW(mp) CDXLScalarProjElem(mp, colref->Id(), mdname);
	CDXLNode *pdxlnPrEl = GPOS_NEW(mp) CDXLNode(mp, pdxlopPrEl);

	// create a scalar identifier for the proj element expression
	CDXLNode *pdxlnScId =
		PdxlnIdent(mp, phmcrdxlnSubplans, nullptr /*phmcrdxlnIndexLookup*/,
				   nullptr /*phmcrulPartColId*/, colref);

	if (EdxlopScalarSubPlan == pdxlnScId->GetOperator()->GetDXLOperator())
	{
		// modify map by replacing subplan entry with the projected
		// column reference so that all subplan references higher up
		// in the DXL tree use the projected col ref
		ReplaceSubplan(mp, phmcrdxlnSubplans, colref, pdxlopPrEl);
	}

	// attach scalar id expression to proj elem
	pdxlnPrEl->AddChild(pdxlnScId);

	return pdxlnPrEl;
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::PdxlnIdent
//
//	@doc:
//		 Create a scalar identifier node from a given col ref
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXLUtils::PdxlnIdent(CMemoryPool *mp,
									  ColRefToDXLNodeMap *phmcrdxlnSubplans,
									  ColRefToDXLNodeMap *phmcrdxlnIndexLookup,
									  ColRefToUlongMap *phmcrulPartColId,
									  const CColRef *colref)
{
	GPOS_ASSERT(nullptr != colref);
	GPOS_ASSERT(nullptr != phmcrdxlnSubplans);

	CDXLNode *dxlnode = phmcrdxlnSubplans->Find(colref);

	if (nullptr != dxlnode)
	{
		dxlnode->AddRef();
		return dxlnode;
	}

	// Check if partition mapping exists (which implies that it is a partitioned
	// table)
	ULONG colid = colref->Id();
	if (nullptr != phmcrulPartColId)
	{
		// if colref doesn't exist in partition mapping, then this scalar ident
		// is an outer ref, and we must look it up in the index outer-ref mapping
		ULONG *pul = phmcrulPartColId->Find(colref);
		if (nullptr == pul)
		{
			CDXLNode *pdxlnIdent = phmcrdxlnIndexLookup->Find(colref);
			GPOS_ASSERT(nullptr != pdxlnIdent);
			pdxlnIdent->AddRef();
			return pdxlnIdent;
		}
		// the colref does exist in the partition mapping, it is therefore NOT
		// an outer ref, and we should create a dxl node
		GPOS_ASSERT(nullptr != pul);
		colid = *pul;
	}
	else
	{
		// scalar ident is not part of partition table, can look up in index
		// directly in index outer-ref mapping
		if (nullptr != phmcrdxlnIndexLookup)
		{
			CDXLNode *pdxlnIdent = phmcrdxlnIndexLookup->Find(colref);
			if (nullptr != pdxlnIdent)
			{
				pdxlnIdent->AddRef();
				return pdxlnIdent;
			}
		}
	}

	CMDName *mdname = GPOS_NEW(mp) CMDName(mp, colref->Name().Pstr());

	IMDId *mdid = colref->RetrieveType()->MDId();
	mdid->AddRef();

	CDXLColRef *dxl_colref =
		GPOS_NEW(mp) CDXLColRef(mdname, colid, mdid, colref->TypeModifier());

	CDXLScalarIdent *dxl_op = GPOS_NEW(mp) CDXLScalarIdent(mp, dxl_colref);
	return GPOS_NEW(mp) CDXLNode(mp, dxl_op);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::PdrgpdatumNulls
//
//	@doc:
//		Create an array of NULL datums for a given array of columns
//
//---------------------------------------------------------------------------
IDatumArray *
CTranslatorExprToDXLUtils::PdrgpdatumNulls(CMemoryPool *mp,
										   CColRefArray *colref_array)
{
	IDatumArray *pdrgpdatum = GPOS_NEW(mp) IDatumArray(mp);

	const ULONG size = colref_array->Size();
	for (ULONG ul = 0; ul < size; ul++)
	{
		CColRef *colref = (*colref_array)[ul];
		const IMDType *pmdtype = colref->RetrieveType();
		IDatum *datum = pmdtype->DatumNull();
		datum->AddRef();
		pdrgpdatum->Append(datum);
	}

	return pdrgpdatum;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::FProjectListMatch
//
//	@doc:
//		Check whether a project list has the same columns in the given array
//		and in the same order
//
//---------------------------------------------------------------------------
BOOL
CTranslatorExprToDXLUtils::FProjectListMatch(CDXLNode *pdxlnPrL,
											 CColRefArray *colref_array)
{
	GPOS_ASSERT(nullptr != pdxlnPrL);
	GPOS_ASSERT(nullptr != colref_array);
	GPOS_ASSERT(EdxlopScalarProjectList ==
				pdxlnPrL->GetOperator()->GetDXLOperator());

	const ULONG length = colref_array->Size();
	if (pdxlnPrL->Arity() != length)
	{
		return false;
	}

	for (ULONG ul = 0; ul < length; ul++)
	{
		CColRef *colref = (*colref_array)[ul];

		CDXLNode *pdxlnPrEl = (*pdxlnPrL)[ul];
		CDXLScalarProjElem *pdxlopPrEl =
			CDXLScalarProjElem::Cast(pdxlnPrEl->GetOperator());

		if (colref->Id() != pdxlopPrEl->Id())
		{
			return false;
		}
	}

	return true;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::PdrgpcrMapColumns
//
//	@doc:
//		Map an array of columns to a new array of columns. The column index is
//		look up in the given hash map, then the corresponding column from
//		the destination array is used
//
//---------------------------------------------------------------------------
CColRefArray *
CTranslatorExprToDXLUtils::PdrgpcrMapColumns(CMemoryPool *mp,
											 CColRefArray *pdrgpcrInput,
											 ColRefToUlongMap *phmcrul,
											 CColRefArray *pdrgpcrMapDest)
{
	GPOS_ASSERT(nullptr != phmcrul);
	GPOS_ASSERT(nullptr != pdrgpcrMapDest);

	if (nullptr == pdrgpcrInput)
	{
		return nullptr;
	}

	CColRefArray *pdrgpcrNew = GPOS_NEW(mp) CColRefArray(mp);

	const ULONG length = pdrgpcrInput->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CColRef *colref = (*pdrgpcrInput)[ul];

		// get column index from hashmap
		ULONG *pul = phmcrul->Find(colref);
		GPOS_ASSERT(nullptr != pul);

		// add corresponding column from dest array
		pdrgpcrNew->Append((*pdrgpcrMapDest)[*pul]);
	}

	return pdrgpcrNew;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::PdxlnResult
//
//	@doc:
//		Create a DXL result node using the given properties, project list,
//		filters, and relational child
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXLUtils::PdxlnResult(CMemoryPool *mp,
									   CDXLPhysicalProperties *dxl_properties,
									   CDXLNode *pdxlnPrL,
									   CDXLNode *filter_dxlnode,
									   CDXLNode *one_time_filter,
									   CDXLNode *child_dxlnode)
{
	CDXLPhysicalResult *dxl_op = GPOS_NEW(mp) CDXLPhysicalResult(mp);
	CDXLNode *pdxlnResult = GPOS_NEW(mp) CDXLNode(mp, dxl_op);
	pdxlnResult->SetProperties(dxl_properties);

	pdxlnResult->AddChild(pdxlnPrL);
	pdxlnResult->AddChild(filter_dxlnode);
	pdxlnResult->AddChild(one_time_filter);

	if (nullptr != child_dxlnode)
	{
		pdxlnResult->AddChild(child_dxlnode);
	}

#ifdef GPOS_DEBUG
	dxl_op->AssertValid(pdxlnResult, false /* validate_children */);
#endif

	return pdxlnResult;
}

// create a DXL Value Scan node
CDXLNode *
CTranslatorExprToDXLUtils::PdxlnValuesScan(
	CMemoryPool *mp, CDXLPhysicalProperties *dxl_properties, CDXLNode *pdxlnPrL,
	IDatum2dArray *pdrgpdrgdatum)
{
	CDXLPhysicalValuesScan *dxl_op = GPOS_NEW(mp) CDXLPhysicalValuesScan(mp);
	CDXLNode *pdxlnValuesScan = GPOS_NEW(mp) CDXLNode(mp, dxl_op);
	pdxlnValuesScan->SetProperties(dxl_properties);

	pdxlnValuesScan->AddChild(pdxlnPrL);

	const ULONG ulTuples = pdrgpdrgdatum->Size();

	for (ULONG ulTuplePos = 0; ulTuplePos < ulTuples; ulTuplePos++)
	{
		IDatumArray *pdrgpdatum = (*pdrgpdrgdatum)[ulTuplePos];
		pdrgpdatum->AddRef();
		const ULONG num_cols = pdrgpdatum->Size();
		CDXLScalarValuesList *values = GPOS_NEW(mp) CDXLScalarValuesList(mp);
		CDXLNode *value_list_dxlnode = GPOS_NEW(mp) CDXLNode(mp, values);

		for (ULONG ulColPos = 0; ulColPos < num_cols; ulColPos++)
		{
			IDatum *datum = (*pdrgpdatum)[ulColPos];
			CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
			const IMDType *pmdtype = md_accessor->RetrieveType(datum->MDId());

			CDXLNode *pdxlnValue =
				GPOS_NEW(mp) CDXLNode(mp, pmdtype->GetDXLOpScConst(mp, datum));
			value_list_dxlnode->AddChild(pdxlnValue);
		}
		pdrgpdatum->Release();
		pdxlnValuesScan->AddChild(value_list_dxlnode);
	}

#ifdef GPOS_DEBUG
	dxl_op->AssertValid(pdxlnValuesScan, true /* validate_children */);
#endif

	return pdxlnValuesScan;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::PdxlnCombineBoolean
//
//	@doc:
//		Combine two boolean expressions using the given boolean operator
//
//---------------------------------------------------------------------------
CDXLNode *
CTranslatorExprToDXLUtils::PdxlnCombineBoolean(CMemoryPool *mp,
											   CDXLNode *first_child_dxlnode,
											   CDXLNode *second_child_dxlnode,
											   EdxlBoolExprType boolexptype)
{
	GPOS_ASSERT(Edxlor == boolexptype || Edxland == boolexptype);

	if (nullptr == first_child_dxlnode)
	{
		return second_child_dxlnode;
	}

	if (nullptr == second_child_dxlnode)
	{
		return first_child_dxlnode;
	}

	return GPOS_NEW(mp)
		CDXLNode(mp, GPOS_NEW(mp) CDXLScalarBoolExpr(mp, boolexptype),
				 first_child_dxlnode, second_child_dxlnode);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::PhmcrulColIndex
//
//	@doc:
//		Build a hashmap based on a column array, where the key is the column
//		and the value is the index of that column in the array
//
//---------------------------------------------------------------------------
ColRefToUlongMap *
CTranslatorExprToDXLUtils::PhmcrulColIndex(CMemoryPool *mp,
										   CColRefArray *colref_array)
{
	ColRefToUlongMap *phmcrul = GPOS_NEW(mp) ColRefToUlongMap(mp);

	const ULONG length = colref_array->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CColRef *colref = (*colref_array)[ul];
		ULONG *pul = GPOS_NEW(mp) ULONG(ul);

		// add to hashmap
#ifdef GPOS_DEBUG
		BOOL fRes =
#endif	// GPOS_DEBUG
			phmcrul->Insert(colref, pul);
		GPOS_ASSERT(fRes);
	}

	return phmcrul;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::SetStats
//
//	@doc:
//		Set the statistics of the dxl operator
//
//---------------------------------------------------------------------------
void
CTranslatorExprToDXLUtils::SetStats(CMemoryPool *mp, CMDAccessor *md_accessor,
									CDXLNode *dxlnode, const IStatistics *stats,
									BOOL fRoot)
{
	if (nullptr != stats && GPOS_FTRACE(EopttraceExtractDXLStats) &&
		(GPOS_FTRACE(EopttraceExtractDXLStatsAllNodes) || fRoot))
	{
		CDXLPhysicalProperties::PdxlpropConvert(dxlnode->GetProperties())
			->SetStats(stats->GetDxlStatsDrvdRelation(mp, md_accessor));
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::SetDirectDispatchInfo
//
//	@doc:
//		Set the direct dispatch info of the dxl operator
//
//---------------------------------------------------------------------------
void
CTranslatorExprToDXLUtils::SetDirectDispatchInfo(
	CMemoryPool *mp, CMDAccessor *md_accessor, CDXLNode *dxlnode,
	CExpression *pexpr, CDistributionSpecArray *pdrgpdsBaseTables)
{
	GPOS_ASSERT(nullptr != dxlnode);
	GPOS_ASSERT(nullptr != pexpr);
	GPOS_ASSERT(nullptr != pdrgpdsBaseTables);

	Edxlopid edxlopid = dxlnode->GetOperator()->GetDXLOperator();
	if (EdxlopPhysicalCTAS == edxlopid || EdxlopPhysicalDML == edxlopid)
	{
		// direct dispatch for CTAS and DML handled elsewhere
		// TODO:  - Oct 15, 2014; unify
		return;
	}

	if (1 != pexpr->DeriveJoinDepth() || 1 != pdrgpdsBaseTables->Size())
	{
		// direct dispatch not supported for join queries
		return;
	}

	CExpressionArray *pexprFilterArray =
		COptCtxt::PoctxtFromTLS()->GetDirectDispatchableFilters();
	ULONG size = pexprFilterArray->Size();

	if (0 == size)
	{
		return;
	}

	CDistributionSpec *pds = (*pdrgpdsBaseTables)[0];

	// go thru all the filters and see if we have one that can
	// give us a direct dispatch.
	// e.g. in the below plan, the CPhysicalFilter is on a non distribution
	// key but CPhysicalIndexScan is on distribution key. So this plan can be
	// direct dispatched.
	//
	//		+--CPhysicalMotionGather
	//			+--CPhysicalFilter
	//			  |--CPhysicalIndexScan
	//			  |  +--CScalarCmp (=)
	//			  |     |--CScalarIdent "dist_key"
	//			  |     +--CScalarConst (5)
	//			  +--CScalarCmp (=)
	//			  |--CScalarIdent "non_dist_key"
	//			  +--CScalarConst (5)

	if (CDistributionSpec::EdtHashed == pds->Edt() ||
		CDistributionSpec::EdtRandom == pds->Edt())
	{
		// direct dispatch supported for scans over
		// hash & random distributed tables
		for (ULONG i = 0; i < size; i++)
		{
			CExpression *pexprFilter = (*pexprFilterArray)[i];
			CPropConstraint *ppc = pexprFilter->DerivePropertyConstraint();
			CDXLDirectDispatchInfo *dxl_direct_dispatch_info = nullptr;

			if (nullptr != ppc->Pcnstr())
			{
				GPOS_ASSERT(nullptr != ppc->Pcnstr());

				if (CDistributionSpec::EdtHashed == pds->Edt())
				{
					CDistributionSpecHashed *pdsHashed =
						CDistributionSpecHashed::PdsConvert(pds);
					CExpressionArray *pdrgpexprHashed = pdsHashed->Pdrgpexpr();

					dxl_direct_dispatch_info = GetDXLDirectDispatchInfo(
						mp, md_accessor, pdrgpexprHashed, ppc->Pcnstr());
				}
				else if (CDistributionSpec::EdtRandom == pds->Edt())
				{
					CConstraint *pcnstr = ppc->Pcnstr();

					CDistributionSpecRandom *pdsRandom =
						CDistributionSpecRandom::PdsConvert(pds);

					// Extracting GpSegmentID for RandDist Table
					const CColRef *pcrDistrCol = pdsRandom->GetGpSegmentId();

					if (pcrDistrCol == nullptr)
					{
						// Direct Dispatch not feasible for this scenario as
						// pcrDistrCol might not exist if gp_segment_id is not defined
						continue;
					}

					CConstraint *pcnstrDistrCol =
						pcnstr->Pcnstr(mp, pcrDistrCol);

					if (pcnstrDistrCol == nullptr)
					{
						// Direct Dispatch not feasible for this scenario
						// as no constraint found on the gp_segment_id
						// Eg: In a query on a random distribute table, if we don't
						// have a condition on gp_segment_id, but we have a condition
						// on another column, in that case this condition
						// shall arise.(select * from bar_randDistr where colm1=5;)
						continue;
					}

					dxl_direct_dispatch_info = GetDXLDirectDispatchInfoRandDist(
						mp, md_accessor, pcrDistrCol, pcnstrDistrCol);
				}

				if (nullptr != dxl_direct_dispatch_info)
				{
					dxlnode->SetDirectDispatchInfo(dxl_direct_dispatch_info);
					break;
				}
			}
		}
	}
}
//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::GetDXLDirectDispatchInfoRandDist
//
//	@doc:
//		Compute the direct dispatch info spec if the table is randomly
//		distributed. Returns NULL if this is not possible
//
//---------------------------------------------------------------------------
CDXLDirectDispatchInfo *
CTranslatorExprToDXLUtils::GetDXLDirectDispatchInfoRandDist(
	CMemoryPool *mp, CMDAccessor *md_accessor, const CColRef *pcrDistrCol,
	CConstraint *pcnstrDistrCol)
{
	// For a random distributed table, we get direct gp_segment_id
	// value in the expression, so we use it as is.
	const BOOL useRawValues = true;

	CDXLDatum2dArray *pdrgpdrgpdxldatum = nullptr;

	if (CPredicateUtils::FConstColumn(pcnstrDistrCol, pcrDistrCol))
	{
		CDXLDatum *dxl_datum = PdxldatumFromPointConstraint(
			mp, md_accessor, pcrDistrCol, pcnstrDistrCol);
		GPOS_ASSERT(nullptr != dxl_datum);

		if (FDirectDispatchable(md_accessor, pcrDistrCol, dxl_datum))
		{
			CDXLDatumArray *pdrgpdxldatum = GPOS_NEW(mp) CDXLDatumArray(mp);

			dxl_datum->AddRef();
			pdrgpdxldatum->Append(dxl_datum);

			pdrgpdrgpdxldatum = GPOS_NEW(mp) CDXLDatum2dArray(mp);
			pdrgpdrgpdxldatum->Append(pdrgpdxldatum);
		}

		dxl_datum->Release();
	}
	else if (CPredicateUtils::FColumnDisjunctionOfConst(pcnstrDistrCol,
														pcrDistrCol))
	{
		pdrgpdrgpdxldatum = PdrgpdrgpdxldatumFromDisjPointConstraint(
			mp, md_accessor, pcrDistrCol, pcnstrDistrCol);
	}

	CRefCount::SafeRelease(pcnstrDistrCol);

	if (nullptr == pdrgpdrgpdxldatum)
	{
		return nullptr;
	}
	return GPOS_NEW(mp) CDXLDirectDispatchInfo(pdrgpdrgpdxldatum, useRawValues);
}
//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::GetDXLDirectDispatchInfo
//
//	@doc:
//		Compute the direct dispatch info spec. Returns NULL if this is not
//		possible
//
//---------------------------------------------------------------------------
CDXLDirectDispatchInfo *
CTranslatorExprToDXLUtils::GetDXLDirectDispatchInfo(
	CMemoryPool *mp, CMDAccessor *md_accessor,
	CExpressionArray *pdrgpexprHashed, CConstraint *pcnstr)
{
	GPOS_ASSERT(nullptr != pdrgpexprHashed);
	GPOS_ASSERT(nullptr != pcnstr);

	const ULONG ulHashExpr = pdrgpexprHashed->Size();
	GPOS_ASSERT(0 < ulHashExpr);

	// If we have single distribution key for table
	if (1 == ulHashExpr)
	{
		CExpression *pexprHashed = (*pdrgpexprHashed)[0];
		return PdxlddinfoSingleDistrKey(mp, md_accessor, pexprHashed, pcnstr);
	}

	BOOL fSuccess = true;
	CDXLDatumArray *pdrgpdxldatum = GPOS_NEW(mp) CDXLDatumArray(mp);

	// If we have multiple distribution keys for the table
	for (ULONG ul = 0; ul < ulHashExpr && fSuccess; ul++)
	{
		CExpression *pexpr = (*pdrgpexprHashed)[ul];
		if (!CUtils::FScalarIdent(pexpr))
		{
			fSuccess = false;
			break;
		}

		const CColRef *pcrDistrCol =
			CScalarIdent::PopConvert(pexpr->Pop())->Pcr();

		CConstraint *pcnstrDistrCol = pcnstr->Pcnstr(mp, pcrDistrCol);

		CDXLDatum *dxl_datum = PdxldatumFromPointConstraint(
			mp, md_accessor, pcrDistrCol, pcnstrDistrCol);
		CRefCount::SafeRelease(pcnstrDistrCol);

		if (nullptr != dxl_datum &&
			FDirectDispatchable(md_accessor, pcrDistrCol, dxl_datum))
		{
			pdrgpdxldatum->Append(dxl_datum);
		}
		else
		{
			CRefCount::SafeRelease(dxl_datum);

			fSuccess = false;
			break;
		}
	}

	if (!fSuccess)
	{
		pdrgpdxldatum->Release();

		return nullptr;
	}

	CDXLDatum2dArray *pdrgpdrgpdxldatum = GPOS_NEW(mp) CDXLDatum2dArray(mp);
	pdrgpdrgpdxldatum->Append(pdrgpdxldatum);
	return GPOS_NEW(mp) CDXLDirectDispatchInfo(pdrgpdrgpdxldatum, false);
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::PdxlddinfoSingleDistrKey
//
//	@doc:
//		Compute the direct dispatch info spec for a single distribution key.
//		Returns NULL if this is not possible
//
//---------------------------------------------------------------------------
CDXLDirectDispatchInfo *
CTranslatorExprToDXLUtils::PdxlddinfoSingleDistrKey(CMemoryPool *mp,
													CMDAccessor *md_accessor,
													CExpression *pexprHashed,
													CConstraint *pcnstr)
{
	GPOS_ASSERT(nullptr != pexprHashed);
	if (!CUtils::FScalarIdent(pexprHashed))
	{
		return nullptr;
	}

	const CColRef *pcrDistrCol =
		CScalarIdent::PopConvert(pexprHashed->Pop())->Pcr();

	BOOL useRawValues = false;
	CConstraint *pcnstrDistrCol = pcnstr->Pcnstr(mp, pcrDistrCol);
	CConstraintInterval *pcnstrInterval;
	// Avoid direct dispatch when pcnstrDistrCol specifies a constant column
	if (!CPredicateUtils::FConstColumn(pcnstrDistrCol, pcrDistrCol) &&
		(pcnstrInterval = dynamic_cast<CConstraintInterval *>(
			 pcnstr->GetConstraintOnSegmentId())) != nullptr)
	{
		if (pcnstrDistrCol != nullptr)
		{
			pcnstrDistrCol->Release();
		}
		// If the constraint is on gp_segment_id then we trick ourselves into
		// considering the constraint as being on a distribution column.
		pcnstrDistrCol = pcnstrInterval;
		pcnstrDistrCol->AddRef();
		pcrDistrCol = pcnstrInterval->Pcr();
		useRawValues = true;
	}

	CDXLDatum2dArray *pdrgpdrgpdxldatum = nullptr;

	if (CPredicateUtils::FConstColumn(pcnstrDistrCol, pcrDistrCol))
	{
		CDXLDatum *dxl_datum = PdxldatumFromPointConstraint(
			mp, md_accessor, pcrDistrCol, pcnstrDistrCol);
		GPOS_ASSERT(nullptr != dxl_datum);

		if (FDirectDispatchable(md_accessor, pcrDistrCol, dxl_datum))

		{
			CDXLDatumArray *pdrgpdxldatum = GPOS_NEW(mp) CDXLDatumArray(mp);

			dxl_datum->AddRef();
			pdrgpdxldatum->Append(dxl_datum);

			pdrgpdrgpdxldatum = GPOS_NEW(mp) CDXLDatum2dArray(mp);
			pdrgpdrgpdxldatum->Append(pdrgpdxldatum);
		}

		dxl_datum->Release();
	}
	else if (CPredicateUtils::FColumnDisjunctionOfConst(pcnstrDistrCol,
														pcrDistrCol))
	{
		pdrgpdrgpdxldatum = PdrgpdrgpdxldatumFromDisjPointConstraint(
			mp, md_accessor, pcrDistrCol, pcnstrDistrCol);
	}

	CRefCount::SafeRelease(pcnstrDistrCol);

	if (nullptr == pdrgpdrgpdxldatum)
	{
		return nullptr;
	}

	return GPOS_NEW(mp) CDXLDirectDispatchInfo(pdrgpdrgpdxldatum, useRawValues);
}


//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::FDirectDispatchable
//
//	@doc:
//		Check if the given constant value for a particular distribution column
// 		can be used to identify which segment to direct dispatch to.
//
//---------------------------------------------------------------------------
BOOL
CTranslatorExprToDXLUtils::FDirectDispatchable(CMDAccessor *md_accessor,
											   const CColRef *pcrDistrCol,
											   const CDXLDatum *dxl_datum)
{
	GPOS_ASSERT(nullptr != pcrDistrCol);
	GPOS_ASSERT(nullptr != dxl_datum);

	IMDId *pmdidDatum = dxl_datum->MDId();
	IMDId *pmdidDistrCol = pcrDistrCol->RetrieveType()->MDId();

	// since all integer values are up-casted to int64, the hash value will be
	// consistent. If either the constant or the distribution column are
	// not integers, then their datatypes must be identical to ensure that
	// the hash value of the constant will point to the right segment.
	BOOL fBothInt =
		CUtils::FIntType(pmdidDistrCol) && CUtils::FIntType(pmdidDatum);

	if (fBothInt || (pmdidDatum->Equals(pmdidDistrCol)))
	{
		return true;
	}
	else
	{
		// if both the IMDId have different oids,
		// then we check if a cast exist between them
		// and if that cast is binary coercible.
		// Eg if datum oid id 25(Text) and DistCol oid is 1043(VarChar)
		// then since a cast is possible and
		// cast is binary coercible, we go ahead with direct dispatch

		const IMDCast *pmdcast_datumToDistrCol;
		const IMDCast *pmdcast_distrColToDatum;

		// Checking if cast exist from datum to distribution column
		GPOS_TRY
		{
			// Pmdcast(,) generates an exception
			// whenever cast is not possible.
			pmdcast_datumToDistrCol =
				md_accessor->Pmdcast(pmdidDatum, pmdidDistrCol);

			if ((pmdcast_datumToDistrCol->IsBinaryCoercible()))
			{
				// cast exist and is between coercible type
				return true;
			}
		}
		GPOS_CATCH_EX(ex)
		{
			GPOS_RESET_EX;
		}
		GPOS_CATCH_END;

		// Checking if cast exist from distribution column to datum
		// eg:explain select gp_segment_id, * from t1_varchar
		// where col1_varchar = 'a'::char;
		GPOS_TRY
		{
			// Pmdcast(,) generates an exception
			// whenever cast is not possible.
			pmdcast_distrColToDatum =
				md_accessor->Pmdcast(pmdidDistrCol, pmdidDatum);

			if ((pmdcast_distrColToDatum->IsBinaryCoercible()))
			{
				// cast exist and is between coercible type
				return true;
			}
		}
		GPOS_CATCH_EX(ex)
		{
			GPOS_RESET_EX;
		}
		GPOS_CATCH_END;

		return false;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::PdxldatumFromPointConstraint
//
//	@doc:
//		Compute a DXL datum from a point constraint. Returns NULL if this is not
//		possible
//
//---------------------------------------------------------------------------
CDXLDatum *
CTranslatorExprToDXLUtils::PdxldatumFromPointConstraint(
	CMemoryPool *mp, CMDAccessor *md_accessor, const CColRef *pcrDistrCol,
	CConstraint *pcnstrDistrCol)
{
	if (!CPredicateUtils::FConstColumn(pcnstrDistrCol, pcrDistrCol))
	{
		return nullptr;
	}

	GPOS_ASSERT(CConstraint::EctInterval == pcnstrDistrCol->Ect());

	CConstraintInterval *pci =
		dynamic_cast<CConstraintInterval *>(pcnstrDistrCol);
	GPOS_ASSERT(1 >= pci->Pdrgprng()->Size());

	CDXLDatum *dxl_datum = nullptr;

	if (1 == pci->Pdrgprng()->Size())
	{
		const CRange *prng = (*pci->Pdrgprng())[0];
		dxl_datum = CTranslatorExprToDXLUtils::GetDatumVal(mp, md_accessor,
														   prng->PdatumLeft());
	}
	else
	{
		GPOS_ASSERT(pci->FIncludesNull());
		dxl_datum = pcrDistrCol->RetrieveType()->GetDXLDatumNull(mp);
	}

	return dxl_datum;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::PdrgpdrgpdxldatumFromDisjPointConstraint
//
//	@doc:
//		Compute an array of DXL datum arrays from a disjunction of point constraints.
//		Returns NULL if this is not possible
//
//---------------------------------------------------------------------------
CDXLDatum2dArray *
CTranslatorExprToDXLUtils::PdrgpdrgpdxldatumFromDisjPointConstraint(
	CMemoryPool *mp, CMDAccessor *md_accessor, const CColRef *pcrDistrCol,
	CConstraint *pcnstrDistrCol)
{
	GPOS_ASSERT(nullptr != pcnstrDistrCol);
	if (CPredicateUtils::FConstColumn(pcnstrDistrCol, pcrDistrCol))
	{
		CDXLDatum2dArray *pdrgpdrgpdxldatum = nullptr;

		CDXLDatum *dxl_datum = PdxldatumFromPointConstraint(
			mp, md_accessor, pcrDistrCol, pcnstrDistrCol);

		if (FDirectDispatchable(md_accessor, pcrDistrCol, dxl_datum))
		{
			CDXLDatumArray *pdrgpdxldatum = GPOS_NEW(mp) CDXLDatumArray(mp);

			dxl_datum->AddRef();
			pdrgpdxldatum->Append(dxl_datum);

			pdrgpdrgpdxldatum = GPOS_NEW(mp) CDXLDatum2dArray(mp);
			pdrgpdrgpdxldatum->Append(pdrgpdxldatum);
		}

		// clean up
		dxl_datum->Release();

		return pdrgpdrgpdxldatum;
	}

	GPOS_ASSERT(CConstraint::EctInterval == pcnstrDistrCol->Ect());

	CConstraintInterval *pcnstrInterval =
		dynamic_cast<CConstraintInterval *>(pcnstrDistrCol);

	CRangeArray *pdrgprng = pcnstrInterval->Pdrgprng();

	const ULONG ulRanges = pdrgprng->Size();
	CDXLDatum2dArray *pdrgpdrgpdxdatum = GPOS_NEW(mp) CDXLDatum2dArray(mp);

	for (ULONG ul = 0; ul < ulRanges; ul++)
	{
		CRange *prng = (*pdrgprng)[ul];
		GPOS_ASSERT(prng->FPoint());
		CDXLDatum *dxl_datum = CTranslatorExprToDXLUtils::GetDatumVal(
			mp, md_accessor, prng->PdatumLeft());

		if (!FDirectDispatchable(md_accessor, pcrDistrCol, dxl_datum))
		{
			// clean up
			dxl_datum->Release();
			pdrgpdrgpdxdatum->Release();

			return nullptr;
		}

		CDXLDatumArray *pdrgpdxldatum = GPOS_NEW(mp) CDXLDatumArray(mp);

		pdrgpdxldatum->Append(dxl_datum);
		pdrgpdrgpdxdatum->Append(pdrgpdxldatum);
	}

	if (pcnstrInterval->FIncludesNull())
	{
		CDXLDatum *dxl_datum = pcrDistrCol->RetrieveType()->GetDXLDatumNull(mp);

		if (!FDirectDispatchable(md_accessor, pcrDistrCol, dxl_datum))
		{
			// clean up
			dxl_datum->Release();
			pdrgpdrgpdxdatum->Release();

			return nullptr;
		}

		CDXLDatumArray *pdrgpdxldatum = GPOS_NEW(mp) CDXLDatumArray(mp);
		pdrgpdxldatum->Append(dxl_datum);
		pdrgpdrgpdxdatum->Append(pdrgpdxldatum);
	}

	if (0 < pdrgpdrgpdxdatum->Size())
	{
		return pdrgpdrgpdxdatum;
	}

	// clean up
	pdrgpdrgpdxdatum->Release();

	return nullptr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::FLocalHashAggStreamSafe
//
//	@doc:
//		Is the aggregate a local hash aggregate that is safe to stream
//
//---------------------------------------------------------------------------
BOOL
CTranslatorExprToDXLUtils::FLocalHashAggStreamSafe(CExpression *pexprAgg)
{
	GPOS_ASSERT(nullptr != pexprAgg);

	COperator::EOperatorId op_id = pexprAgg->Pop()->Eopid();

	if (COperator::EopPhysicalHashAgg != op_id &&
		COperator::EopPhysicalHashAggDeduplicate != op_id)
	{
		// not a hash aggregate
		return false;
	}

	CPhysicalAgg *popAgg = CPhysicalAgg::PopConvert(pexprAgg->Pop());

	// is a local hash aggregate and it generates duplicates (therefore safe to stream)
	return (COperator::EgbaggtypeLocal == popAgg->Egbaggtype()) &&
		   popAgg->FGeneratesDuplicates();
}

//---------------------------------------------------------------------------
//	@function:
//		CTranslatorExprToDXLUtils::ExtractCastFuncMdids
//
//	@doc:
//		If operator is a scalar cast/scalar func extract type and function
//
//---------------------------------------------------------------------------
void
CTranslatorExprToDXLUtils::ExtractCastFuncMdids(COperator *pop,
												IMDId **ppmdidType,
												IMDId **ppmdidCastFunc)
{
	GPOS_ASSERT(nullptr != pop);
	GPOS_ASSERT(nullptr != ppmdidType);
	GPOS_ASSERT(nullptr != ppmdidCastFunc);

	if (COperator::EopScalarCast != pop->Eopid() &&
		COperator::EopScalarFunc != pop->Eopid())
	{
		// not a cast or allowed func
		return;
	}
	if (COperator::EopScalarCast == pop->Eopid())
	{
		CScalarCast *popCast = CScalarCast::PopConvert(pop);
		*ppmdidType = popCast->MdidType();
		*ppmdidCastFunc = popCast->FuncMdId();
	}
	else
	{
		GPOS_ASSERT(COperator::EopScalarFunc == pop->Eopid());
		CScalarFunc *popFunc = CScalarFunc::PopConvert(pop);
		*ppmdidType = popFunc->MdidType();
		*ppmdidCastFunc = popFunc->FuncMdId();
	}
}

BOOL
CTranslatorExprToDXLUtils::FDXLOpExists(const CDXLOperator *pop,
										const gpdxl::Edxlopid *peopid,
										ULONG ulOps)
{
	GPOS_ASSERT(nullptr != pop);
	GPOS_ASSERT(nullptr != peopid);

	gpdxl::Edxlopid op_id = pop->GetDXLOperator();
	for (ULONG ul = 0; ul < ulOps; ul++)
	{
		if (op_id == peopid[ul])
		{
			return true;
		}
	}

	return false;
}

BOOL
CTranslatorExprToDXLUtils::FHasDXLOp(const CDXLNode *dxlnode,
									 const gpdxl::Edxlopid *peopid, ULONG ulOps)
{
	GPOS_CHECK_STACK_SIZE;
	GPOS_ASSERT(nullptr != dxlnode);
	GPOS_ASSERT(nullptr != peopid);

	if (FDXLOpExists(dxlnode->GetOperator(), peopid, ulOps))
	{
		return true;
	}

	// recursively check children
	const ULONG arity = dxlnode->Arity();

	for (ULONG ul = 0; ul < arity; ul++)
	{
		if (FHasDXLOp((*dxlnode)[ul], peopid, ulOps))
		{
			return true;
		}
	}

	return false;
}

BOOL
CTranslatorExprToDXLUtils::FProjListContainsSubplanWithBroadCast(
	CDXLNode *pdxlnPrjList)
{
	if (pdxlnPrjList->GetOperator()->GetDXLOperator() == EdxlopScalarSubPlan)
	{
		gpdxl::Edxlopid rgeopidMotion[] = {EdxlopPhysicalMotionBroadcast};
		return FHasDXLOp(pdxlnPrjList, rgeopidMotion,
						 GPOS_ARRAY_SIZE(rgeopidMotion));
	}

	const ULONG arity = pdxlnPrjList->Arity();

	for (ULONG ul = 0; ul < arity; ul++)
	{
		if (FProjListContainsSubplanWithBroadCast((*pdxlnPrjList)[ul]))
		{
			return true;
		}
	}

	return false;
}

void
CTranslatorExprToDXLUtils::ExtractIdentColIds(CDXLNode *dxlnode, CBitSet *pbs)
{
	if (dxlnode->GetOperator()->GetDXLOperator() == EdxlopScalarIdent)
	{
		const CDXLColRef *dxl_colref =
			CDXLScalarIdent::Cast(dxlnode->GetOperator())->GetDXLColRef();
		pbs->ExchangeSet(dxl_colref->Id());
	}

	ULONG arity = dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		ExtractIdentColIds((*dxlnode)[ul], pbs);
	}
}

BOOL
CTranslatorExprToDXLUtils::FMotionHazard(CMemoryPool *mp, CDXLNode *dxlnode,
										 const gpdxl::Edxlopid *peopid,
										 ULONG ulOps, CBitSet *pbsPrjCols)
{
	GPOS_ASSERT(pbsPrjCols);

	// non-streaming operator/Gather motion neutralizes any motion hazard that its subtree imposes
	// hence stop recursing further
	if (FMotionHazardSafeOp(dxlnode))
	{
		return false;
	}

	if (FDXLOpExists(dxlnode->GetOperator(), peopid, ulOps))
	{
		// check if the current motion node projects any column from the
		// input project list.
		// If yes, then we have detected a motion hazard for the parent Result node.
		CBitSet *pbsPrjList = GPOS_NEW(mp) CBitSet(mp);
		ExtractIdentColIds((*dxlnode)[0], pbsPrjList);
		BOOL fDisJoint = pbsPrjCols->IsDisjoint(pbsPrjList);
		pbsPrjList->Release();

		return !fDisJoint;
	}

	// recursively check children
	const ULONG arity = dxlnode->Arity();

	// In ORCA, inner child of Hash Join is always exhausted first,
	// so only check the outer child for motions
	if (dxlnode->GetOperator()->GetDXLOperator() == EdxlopPhysicalHashJoin)
	{
		if (FMotionHazard(mp, (*dxlnode)[EdxlhjIndexHashLeft], peopid, ulOps,
						  pbsPrjCols))
		{
			return true;
		}
	}
	else
	{
		for (ULONG ul = 0; ul < arity; ul++)
		{
			if (FMotionHazard(mp, (*dxlnode)[ul], peopid, ulOps, pbsPrjCols))
			{
				return true;
			}
		}
	}

	return false;
}

BOOL
CTranslatorExprToDXLUtils::FMotionHazardSafeOp(CDXLNode *dxlnode)
{
	BOOL fMotionHazardSafeOp = false;
	Edxlopid edxlop = dxlnode->GetOperator()->GetDXLOperator();

	switch (edxlop)
	{
		case EdxlopPhysicalSort:
		case EdxlopPhysicalMotionGather:
		case EdxlopPhysicalMaterialize:
			fMotionHazardSafeOp = true;
			break;

		case EdxlopPhysicalAgg:
		{
			CDXLPhysicalAgg *pdxlnPhysicalAgg =
				CDXLPhysicalAgg::Cast(dxlnode->GetOperator());
			if (pdxlnPhysicalAgg->GetAggStrategy() == EdxlaggstrategyHashed)
			{
				fMotionHazardSafeOp = true;
			}
		}
		break;

		default:
			break;
	}

	return fMotionHazardSafeOp;
}

BOOL
CTranslatorExprToDXLUtils::FDirectDispatchableFilter(CExpression *pexprFilter)
{
	GPOS_ASSERT(nullptr != pexprFilter);

	CExpression *pexprChild = (*pexprFilter)[0];
	COperator *pop = pexprChild->Pop();

	// find the first child or grandchild of filter which is not
	// a Project, Filter or PhysicalComputeScalar (result node)
	// if it is a scan, then this Filter is direct dispatchable
	while (COperator::EopPhysicalPartitionSelector == pop->Eopid() ||
		   COperator::EopPhysicalFilter == pop->Eopid() ||
		   COperator::EopPhysicalComputeScalar == pop->Eopid())
	{
		pexprChild = (*pexprChild)[0];
		pop = pexprChild->Pop();
	}

	return (CUtils::FPhysicalScan(pop));
}

// EOF

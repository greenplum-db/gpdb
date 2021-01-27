//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarAggref.cpp
//
//	@doc:
//		Implementation of DXL AggRef
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarAggref.h"

#include "gpopt/mdcache/CMDAccessor.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/md/IMDAggregate.h"

using namespace gpopt;
using namespace gpmd;
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAggref::CDXLScalarAggref
//
//	@doc:
//		Constructs an AggRef node
//
//---------------------------------------------------------------------------
CDXLScalarAggref::CDXLScalarAggref(CMemoryPool *mp, IMDId *agg_func_mdid,
								   IMDId *resolved_rettype_mdid,
								   BOOL is_distinct, EdxlAggrefStage agg_stage)
	: CDXLScalar(mp),
	  m_agg_func_mdid(agg_func_mdid),
	  m_resolved_rettype_mdid(resolved_rettype_mdid),
	  m_is_distinct(is_distinct),
	  m_agg_stage(agg_stage)
{
	GPOS_ASSERT(NULL != agg_func_mdid);
	GPOS_ASSERT_IMP(NULL != resolved_rettype_mdid,
					resolved_rettype_mdid->IsValid());
	GPOS_ASSERT(m_agg_func_mdid->IsValid());
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAggref::~CDXLScalarAggref
//
//	@doc:
//		dtor
//
//---------------------------------------------------------------------------
CDXLScalarAggref::~CDXLScalarAggref()
{
	m_agg_func_mdid->Release();
	CRefCount::SafeRelease(m_resolved_rettype_mdid);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAggref::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarAggref::GetDXLOperator() const
{
	return EdxlopScalarAggref;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAggref::GetDXLAggStage
//
//	@doc:
//		AggRef AggStage
//
//---------------------------------------------------------------------------
EdxlAggrefStage
CDXLScalarAggref::GetDXLAggStage() const
{
	return m_agg_stage;
}
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAggref::PstrAggStage
//
//	@doc:
//		AggRef AggStage
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarAggref::GetDXLStrAggStage() const
{
	switch (m_agg_stage)
	{
		case EdxlaggstageNormal:
			return CDXLTokens::GetDXLTokenStr(EdxltokenAggrefStageNormal);
		case EdxlaggstagePartial:
			return CDXLTokens::GetDXLTokenStr(EdxltokenAggrefStagePartial);
		case EdxlaggstageIntermediate:
			return CDXLTokens::GetDXLTokenStr(EdxltokenAggrefStageIntermediate);
		case EdxlaggstageFinal:
			return CDXLTokens::GetDXLTokenStr(EdxltokenAggrefStageFinal);
		default:
			GPOS_ASSERT(!"Unrecognized aggregate stage");
			return NULL;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAggref::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarAggref::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarAggref);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAggref::GetDXLAggFuncMDid
//
//	@doc:
//		Returns function id
//
//---------------------------------------------------------------------------
IMDId *
CDXLScalarAggref::GetDXLAggFuncMDid() const
{
	return m_agg_func_mdid;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAggref::GetDXLResolvedRetTypeMDid
//
//	@doc:
//		Returns resolved type id
//
//---------------------------------------------------------------------------
IMDId *
CDXLScalarAggref::GetDXLResolvedRetTypeMDid() const
{
	return m_resolved_rettype_mdid;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAggref::IsDistinct
//
//	@doc:
//		TRUE if it's agg(DISTINCT ...)
//
//---------------------------------------------------------------------------
BOOL
CDXLScalarAggref::IsDistinct() const
{
	return m_is_distinct;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAggref::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarAggref::SerializeToDXL(CXMLSerializer *xml_serializer,
								 const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	m_agg_func_mdid->Serialize(xml_serializer,
							   CDXLTokens::GetDXLTokenStr(EdxltokenAggrefOid));
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenAggrefDistinct), m_is_distinct);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenAggrefStage), GetDXLStrAggStage());
	if (NULL != m_resolved_rettype_mdid)
	{
		m_resolved_rettype_mdid->Serialize(
			xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenTypeId));
	}
	dxlnode->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAggref::HasBoolResult
//
//	@doc:
//		Does the operator return a boolean result
//
//---------------------------------------------------------------------------
BOOL
CDXLScalarAggref::HasBoolResult(CMDAccessor *md_accessor) const
{
	const IMDAggregate *pmdagg = md_accessor->RetrieveAgg(m_agg_func_mdid);
	return (
		IMDType::EtiBool ==
		md_accessor->RetrieveType(pmdagg->GetResultTypeMdid())->GetDatumType());
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarAggref::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarAggref::AssertValid(const CDXLNode *dxlnode,
							  BOOL validate_children) const
{
	EdxlAggrefStage aggrefstage =
		((CDXLScalarAggref *) dxlnode->GetOperator())->GetDXLAggStage();

	GPOS_ASSERT((EdxlaggstageFinal >= aggrefstage) &&
				(EdxlaggstageNormal <= aggrefstage));

	const ULONG arity = dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ++ul)
	{
		CDXLNode *aggref_child_dxl = (*dxlnode)[ul];
		GPOS_ASSERT(EdxloptypeScalar ==
					aggref_child_dxl->GetOperator()->GetDXLOperatorType());

		if (validate_children)
		{
			aggref_child_dxl->GetOperator()->AssertValid(aggref_child_dxl,
														 validate_children);
		}
	}
}
#endif	// GPOS_DEBUG

// EOF

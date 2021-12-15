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
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLScalarValuesList.h"
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
								   BOOL is_distinct, EdxlAggrefStage agg_stage,
								   EdxlAggrefKind aggkind,
								   ULongPtrArray *argtypes)
	: CDXLScalar(mp),
	  m_agg_func_mdid(agg_func_mdid),
	  m_resolved_rettype_mdid(resolved_rettype_mdid),
	  m_is_distinct(is_distinct),
	  m_agg_stage(agg_stage),
	  m_aggkind(aggkind),
	  m_argtypes(argtypes)
{
	GPOS_ASSERT(nullptr != agg_func_mdid);
	GPOS_ASSERT_IMP(nullptr != resolved_rettype_mdid,
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
	CRefCount::SafeRelease(m_argtypes);
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
			return nullptr;
	}
}

const CWStringConst *
CDXLScalarAggref::GetDXLStrAggKind() const
{
	switch (m_aggkind)
	{
		case EdxlaggkindNormal:
			return CDXLTokens::GetDXLTokenStr(EdxltokenAggrefKindNormal);
		case EdxlaggkindOrderedSet:
			return CDXLTokens::GetDXLTokenStr(EdxltokenAggrefKindOrderedSet);
		case EdxlaggkindHypothetical:
			return CDXLTokens::GetDXLTokenStr(EdxltokenAggrefKindHypothetical);
		default:
			GPOS_ASSERT(!"Unrecognized aggregate kind");
			return nullptr;
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
//     @function:
//             CDXLScalarAggref::SerializeValuesListChildToDXL
//
//     @doc:
//             Serialize child CDXLScalarValuesList node in DXL format. Param index is
//             one based.
//
//---------------------------------------------------------------------------
void
CDXLScalarAggref::SerializeValuesListChildToDXL(CXMLSerializer *xml_serializer,
												const CDXLNode *dxlnode,
												ULONG index,
												const CHAR *attr_name) const
{
	CDXLScalarValuesList *child =
		CDXLScalarValuesList::Cast(((*dxlnode)[index])->GetOperator());
	child->SerializeToDXL(xml_serializer, (*dxlnode)[index], attr_name);
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
	if (nullptr != m_resolved_rettype_mdid)
	{
		m_resolved_rettype_mdid->Serialize(
			xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenTypeId));
	}
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenAggrefKind), GetDXLStrAggKind());

	CWStringDynamic *argtypes = CDXLUtils::Serialize(m_mp, m_argtypes);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenAggrefArgTypes), argtypes);
	GPOS_DELETE(argtypes);

	SerializeValuesListChildToDXL(xml_serializer, dxlnode, 0, "aggargs");
	SerializeValuesListChildToDXL(xml_serializer, dxlnode, 1, "aggdirectargs");
	SerializeValuesListChildToDXL(xml_serializer, dxlnode, 2, "aggorder");
	SerializeValuesListChildToDXL(xml_serializer, dxlnode, 3, "aggdistinct");

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

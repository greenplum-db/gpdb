//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLScalarWindowRef.cpp
//
//	@doc:
//		Implementation of DXL WindowRef
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarWindowRef.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

#include "gpopt/mdcache/CMDAccessor.h"
#include "naucrates/md/IMDAggregate.h"

using namespace gpopt;
using namespace gpmd;
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarWindowRef::CDXLScalarWindowRef
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLScalarWindowRef::CDXLScalarWindowRef(CMemoryPool *mp, IMDId *mdid_func,
										 IMDId *mdid_return_type,
										 BOOL is_distinct, BOOL is_star_arg,
										 BOOL is_simple_agg,
										 EdxlWinStage dxl_win_stage,
										 ULONG ulWinspecPosition)
	: CDXLScalar(mp),
	  m_func_mdid(mdid_func),
	  m_return_type_mdid(mdid_return_type),
	  m_is_distinct(is_distinct),
	  m_is_star_arg(is_star_arg),
	  m_is_simple_agg(is_simple_agg),
	  m_dxl_win_stage(dxl_win_stage),
	  m_win_spec_pos(ulWinspecPosition)
{
	GPOS_ASSERT(m_func_mdid->IsValid());
	GPOS_ASSERT(m_return_type_mdid->IsValid());
	GPOS_ASSERT(EdxlwinstageSentinel != m_dxl_win_stage);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarWindowRef::~CDXLScalarWindowRef
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLScalarWindowRef::~CDXLScalarWindowRef()
{
	m_func_mdid->Release();
	m_return_type_mdid->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarWindowRef::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarWindowRef::GetDXLOperator() const
{
	return EdxlopScalarWindowRef;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarWindowRef::GetWindStageStr
//
//	@doc:
//		Return window stage
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarWindowRef::GetWindStageStr() const
{
	GPOS_ASSERT(EdxlwinstageSentinel > m_dxl_win_stage);
	ULONG win_stage_token_mapping[][2] = {
		{EdxlwinstageImmediate, EdxltokenWindowrefStageImmediate},
		{EdxlwinstagePreliminary, EdxltokenWindowrefStagePreliminary},
		{EdxlwinstageRowKey, EdxltokenWindowrefStageRowKey}};

	const ULONG arity = GPOS_ARRAY_SIZE(win_stage_token_mapping);
	for (ULONG ul = 0; ul < arity; ul++)
	{
		ULONG *element = win_stage_token_mapping[ul];
		if ((ULONG) m_dxl_win_stage == element[0])
		{
			Edxltoken dxl_token = (Edxltoken) element[1];
			return CDXLTokens::GetDXLTokenStr(dxl_token);
			break;
		}
	}

	GPOS_ASSERT(!"Unrecognized window stage");
	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarWindowRef::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarWindowRef::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarWindowref);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarWindowRef::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarWindowRef::SerializeToDXL(CXMLSerializer *xml_serializer,
									const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	m_func_mdid->Serialize(xml_serializer,
						   CDXLTokens::GetDXLTokenStr(EdxltokenWindowrefOid));
	m_return_type_mdid->Serialize(xml_serializer,
								  CDXLTokens::GetDXLTokenStr(EdxltokenTypeId));
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenWindowrefDistinct), m_is_distinct);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenWindowrefStarArg), m_is_star_arg);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenWindowrefSimpleAgg),
		m_is_simple_agg);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenWindowrefStrategy),
		GetWindStageStr());
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenWindowrefWinSpecPos),
		m_win_spec_pos);

	dxlnode->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarWindowRef::HasBoolResult
//
//	@doc:
//		Does the operator return a boolean result
//
//---------------------------------------------------------------------------
BOOL
CDXLScalarWindowRef::HasBoolResult(CMDAccessor *md_accessor) const
{
	IMDId *mdid = md_accessor->RetrieveFunc(m_func_mdid)->GetResultTypeMdid();
	return (IMDType::EtiBool ==
			md_accessor->RetrieveType(mdid)->GetDatumType());
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarWindowRef::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarWindowRef::AssertValid(const CDXLNode *dxlnode,
								 BOOL validate_children) const
{
	EdxlWinStage edxlwinrefstage =
		((CDXLScalarWindowRef *) dxlnode->GetOperator())->GetDxlWinStage();

	GPOS_ASSERT((EdxlwinstageSentinel >= edxlwinrefstage));

	const ULONG arity = dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ++ul)
	{
		CDXLNode *dxlnode_winref_arg = (*dxlnode)[ul];
		GPOS_ASSERT(EdxloptypeScalar ==
					dxlnode_winref_arg->GetOperator()->GetDXLOperatorType());

		if (validate_children)
		{
			dxlnode_winref_arg->GetOperator()->AssertValid(dxlnode_winref_arg,
														   validate_children);
		}
	}
}
#endif	// GPOS_DEBUG

// EOF

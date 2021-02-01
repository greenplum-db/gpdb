//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLLogicalTVF.cpp
//
//	@doc:
//		Implementation of DXL table-valued function
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLLogicalTVF.h"

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalTVF::CDXLLogicalTVF
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLLogicalTVF::CDXLLogicalTVF(CMemoryPool *mp, IMDId *mdid_func,
							   IMDId *mdid_return_type, CMDName *mdname,
							   CDXLColDescrArray *pdrgdxlcd)
	: CDXLLogical(mp),
	  m_func_mdid(mdid_func),
	  m_return_type_mdid(mdid_return_type),
	  m_mdname(mdname),
	  m_dxl_col_descr_array(pdrgdxlcd)
{
	GPOS_ASSERT(m_func_mdid->IsValid());
	GPOS_ASSERT(m_return_type_mdid->IsValid());
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalTVF::~CDXLLogicalTVF
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLLogicalTVF::~CDXLLogicalTVF()
{
	m_dxl_col_descr_array->Release();
	m_func_mdid->Release();
	m_return_type_mdid->Release();
	GPOS_DELETE(m_mdname);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalTVF::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLLogicalTVF::GetDXLOperator() const
{
	return EdxlopLogicalTVF;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalTVF::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLLogicalTVF::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenLogicalTVF);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalTVF::Arity
//
//	@doc:
//		Return number of return columns
//
//---------------------------------------------------------------------------
ULONG
CDXLLogicalTVF::Arity() const
{
	return m_dxl_col_descr_array->Size();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalTVF::GetColumnDescrAt
//
//	@doc:
//		Get the column descriptor at the given position
//
//---------------------------------------------------------------------------
const CDXLColDescr *
CDXLLogicalTVF::GetColumnDescrAt(ULONG ul) const
{
	return (*m_dxl_col_descr_array)[ul];
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalTVF::IsColDefined
//
//	@doc:
//		Check if given column is defined by operator
//
//---------------------------------------------------------------------------
BOOL
CDXLLogicalTVF::IsColDefined(ULONG colid) const
{
	const ULONG size = Arity();
	for (ULONG ulDescr = 0; ulDescr < size; ulDescr++)
	{
		ULONG id = GetColumnDescrAt(ulDescr)->Id();
		if (id == colid)
		{
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalTVF::SerializeToDXL
//
//	@doc:
//		Serialize function descriptor in DXL format
//
//---------------------------------------------------------------------------
void
CDXLLogicalTVF::SerializeToDXL(CXMLSerializer *xml_serializer,
							   const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	m_func_mdid->Serialize(xml_serializer,
						   CDXLTokens::GetDXLTokenStr(EdxltokenFuncId));
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenName),
								 m_mdname->GetMDName());
	m_return_type_mdid->Serialize(xml_serializer,
								  CDXLTokens::GetDXLTokenStr(EdxltokenTypeId));

	// serialize columns
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenColumns));
	GPOS_ASSERT(NULL != m_dxl_col_descr_array);

	for (ULONG ul = 0; ul < Arity(); ul++)
	{
		CDXLColDescr *pdxlcd = (*m_dxl_col_descr_array)[ul];
		pdxlcd->SerializeToDXL(xml_serializer);
	}

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenColumns));

	// serialize arguments
	dxlnode->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalTVF::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLLogicalTVF::AssertValid(const CDXLNode *dxlnode,
							BOOL validate_children) const
{
	// assert validity of function id and return type
	GPOS_ASSERT(NULL != m_func_mdid);
	GPOS_ASSERT(NULL != m_return_type_mdid);

	const ULONG arity = dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ++ul)
	{
		CDXLNode *dxlnode_arg = (*dxlnode)[ul];
		GPOS_ASSERT(EdxloptypeScalar ==
					dxlnode_arg->GetOperator()->GetDXLOperatorType());

		if (validate_children)
		{
			dxlnode_arg->GetOperator()->AssertValid(dxlnode_arg,
													validate_children);
		}
	}
}

#endif	// GPOS_DEBUG


// EOF

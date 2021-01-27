//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLStatsDerivedRelation.cpp
//
//	@doc:
//		Implementation of the class for representing DXL derived relation statistics
//---------------------------------------------------------------------------

#include "naucrates/md/CDXLStatsDerivedRelation.h"

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpdxl;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CDXLStatsDerivedRelation::CDXLStatsDerivedRelation
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLStatsDerivedRelation::CDXLStatsDerivedRelation(
	CDouble rows, BOOL is_empty,
	CDXLStatsDerivedColumnArray *dxl_stats_derived_col_array)
	: m_rows(rows),
	  m_empty(is_empty),
	  m_dxl_stats_derived_col_array(dxl_stats_derived_col_array)
{
	GPOS_ASSERT(NULL != dxl_stats_derived_col_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLStatsDerivedRelation::~CDXLStatsDerivedRelation
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLStatsDerivedRelation::~CDXLStatsDerivedRelation()
{
	m_dxl_stats_derived_col_array->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLStatsDerivedRelation::GetDXLStatsDerivedColArray
//
//	@doc:
//		Returns the array of derived columns stats
//
//---------------------------------------------------------------------------
const CDXLStatsDerivedColumnArray *
CDXLStatsDerivedRelation::GetDXLStatsDerivedColArray() const
{
	return m_dxl_stats_derived_col_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLStatsDerivedRelation::Serialize
//
//	@doc:
//		Serialize bucket in DXL format
//
//---------------------------------------------------------------------------
void
CDXLStatsDerivedRelation::Serialize(CXMLSerializer *xml_serializer) const
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenStatsDerivedRelation));

	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenRows),
								 m_rows);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenEmptyRelation), m_empty);

	const ULONG arity = m_dxl_stats_derived_col_array->Size();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		GPOS_CHECK_ABORT;

		CDXLStatsDerivedColumn *derived_col_stats_dxl =
			(*m_dxl_stats_derived_col_array)[ul];
		derived_col_stats_dxl->Serialize(xml_serializer);

		GPOS_CHECK_ABORT;
	}

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenStatsDerivedRelation));
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLStatsDerivedRelation::DebugPrint
//
//	@doc:
//		Debug print of the bucket object
//
//---------------------------------------------------------------------------
void
CDXLStatsDerivedRelation::DebugPrint(IOstream &os) const
{
	os << "Rows: " << Rows() << std::endl;

	os << "Empty: " << IsEmpty() << std::endl;
}

#endif	// GPOS_DEBUG

// EOF

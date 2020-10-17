//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLOperatorCost.cpp
//
//	@doc:
//		Class for representing cost estimates in physical operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLOperatorCost.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

CDXLOperatorCost::CDXLOperatorCost(CWStringDynamic *startup_cost_str,
								   CWStringDynamic *total_cost_str,
								   CWStringDynamic *rows_out_str,
								   CWStringDynamic *width_str)
	: m_startup_cost_str(startup_cost_str),
	  m_total_cost_str(total_cost_str),
	  m_rows_out_str(rows_out_str),
	  m_width_str(width_str)
{
}

CDXLOperatorCost::~CDXLOperatorCost()
{
	GPOS_DELETE(m_startup_cost_str);
	GPOS_DELETE(m_total_cost_str);
	GPOS_DELETE(m_rows_out_str);
	GPOS_DELETE(m_width_str);
}

const CWStringDynamic *
CDXLOperatorCost::GetStartUpCostStr() const
{
	return m_startup_cost_str;
}

const CWStringDynamic *
CDXLOperatorCost::GetTotalCostStr() const
{
	return m_total_cost_str;
}

const CWStringDynamic *
CDXLOperatorCost::GetRowsOutStr() const
{
	return m_rows_out_str;
}

const CWStringDynamic *
CDXLOperatorCost::GetWidthStr() const
{
	return m_width_str;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorCost::SetRows
//
//	@doc:
//		Set the number of rows
//
//---------------------------------------------------------------------------
void
CDXLOperatorCost::SetRows(CWStringDynamic *rows_str)
{
	GPOS_ASSERT(NULL != rows_str);
	GPOS_DELETE(m_rows_out_str);
	m_rows_out_str = rows_str;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLOperatorCost::SetCost
//
//	@doc:
//		Set the total cost
//
//---------------------------------------------------------------------------
void
CDXLOperatorCost::SetCost(CWStringDynamic *cost_str)
{
	GPOS_ASSERT(NULL != cost_str);
	GPOS_DELETE(m_total_cost_str);
	m_total_cost_str = cost_str;
}

void
CDXLOperatorCost::SerializeToDXL(CXMLSerializer *xml_serializer) const
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenCost));

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenStartupCost), m_startup_cost_str);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenTotalCost),
								 m_total_cost_str);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenRows),
								 m_rows_out_str);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenWidth),
								 m_width_str);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenCost));
}

// EOF

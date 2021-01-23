//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLProperties.cpp
//
//	@doc:
//		Implementation of properties of DXL operators
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLProperties.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLProperties::CDXLProperties
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLProperties::CDXLProperties() : m_dxl_stats_derived_relation(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLProperties::~CDXLProperties
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLProperties::~CDXLProperties()
{
	CRefCount::SafeRelease(m_dxl_stats_derived_relation);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLProperties::SetStats
//
//	@doc:
//		Set operator properties
//
//---------------------------------------------------------------------------
void
CDXLProperties::SetStats(CDXLStatsDerivedRelation *dxl_stats_derived_relation)
{
	// allow setting properties only once
	GPOS_ASSERT(NULL == m_dxl_stats_derived_relation);
	GPOS_ASSERT(NULL != dxl_stats_derived_relation);
	m_dxl_stats_derived_relation = dxl_stats_derived_relation;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLProperties::GetDxlStatsDrvdRelation
//
//	@doc:
//		Return operator's statistical information
//
//---------------------------------------------------------------------------
const CDXLStatsDerivedRelation *
CDXLProperties::GetDxlStatsDrvdRelation() const
{
	return m_dxl_stats_derived_relation;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLProperties::SerializePropertiesToDXL
//
//	@doc:
//		Serialize operator statistics in DXL format
//
//---------------------------------------------------------------------------
void
CDXLProperties::SerializePropertiesToDXL(CXMLSerializer *xml_serializer) const
{
	SerializeStatsToDXL(xml_serializer);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLProperties::SerializeStatsToDXL
//
//	@doc:
//		Serialize operator statistics in DXL format
//
//---------------------------------------------------------------------------
void
CDXLProperties::SerializeStatsToDXL(CXMLSerializer *xml_serializer) const
{
	if (NULL != m_dxl_stats_derived_relation)
	{
		m_dxl_stats_derived_relation->Serialize(xml_serializer);
	}
}

// EOF

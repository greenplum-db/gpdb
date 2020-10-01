//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CDXLDirectDispatchInfo.cpp
//
//	@doc:
//		Implementation of DXL datum of types having LINT mapping
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLDirectDispatchInfo.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLDirectDispatchInfo::CDXLDirectDispatchInfo
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLDirectDispatchInfo::CDXLDirectDispatchInfo(
	CDXLDatum2dArray *dispatch_identifer_datum_array, BOOL contains_raw_values)
	: m_dispatch_identifer_datum_array(dispatch_identifer_datum_array),
	  m_contains_raw_values(contains_raw_values)
{
	GPOS_ASSERT(NULL != dispatch_identifer_datum_array);

#ifdef GPOS_DEBUG
	const ULONG length = dispatch_identifer_datum_array->Size();
	if (0 < length)
	{
		ULONG num_of_datums = ((*dispatch_identifer_datum_array)[0])->Size();
		for (ULONG idx = 1; idx < length; idx++)
		{
			GPOS_ASSERT(num_of_datums ==
						((*dispatch_identifer_datum_array)[idx])->Size());
		}
	}
#endif	// GPOS_DEBUG
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLDirectDispatchInfo::~CDXLDirectDispatchInfo
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLDirectDispatchInfo::~CDXLDirectDispatchInfo()
{
	m_dispatch_identifer_datum_array->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLDirectDispatchInfo::Serialize
//
//	@doc:
//		Serialize direct dispatch info in DXL format
//
//---------------------------------------------------------------------------
void
CDXLDirectDispatchInfo::Serialize(CXMLSerializer *xml_serializer)
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenDirectDispatchInfo));

	if (m_contains_raw_values)
	{
		// Output attribute iff true. Default false keeps backward compatability
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenDirectDispatchIsRaw),
			m_contains_raw_values);
	}

	const ULONG num_of_dispatch_identifiers =
		(m_dispatch_identifer_datum_array == NULL)
			? 0
			: m_dispatch_identifer_datum_array->Size();

	for (ULONG idx1 = 0; idx1 < num_of_dispatch_identifiers; idx1++)
	{
		xml_serializer->OpenElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			CDXLTokens::GetDXLTokenStr(EdxltokenDirectDispatchKeyValue));

		CDXLDatumArray *dispatch_identifier_array =
			(*m_dispatch_identifer_datum_array)[idx1];

		const ULONG num_of_datums = dispatch_identifier_array->Size();
		for (ULONG idx2 = 0; idx2 < num_of_datums; idx2++)
		{
			CDXLDatum *dxl_datum = (*dispatch_identifier_array)[idx2];
			dxl_datum->Serialize(xml_serializer,
								 CDXLTokens::GetDXLTokenStr(EdxltokenDatum));
		}

		xml_serializer->CloseElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			CDXLTokens::GetDXLTokenStr(EdxltokenDirectDispatchKeyValue));
	}

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenDirectDispatchInfo));
}

// EOF

//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLSpoolInfo.cpp
//
//	@doc:
//		Implementation of DXL sorting columns for sort and motion operator nodes
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLSpoolInfo.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"


using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLSpoolInfo::CDXLSpoolInfo
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLSpoolInfo::CDXLSpoolInfo(ULONG ulSpoolId, Edxlspooltype edxlspstype,
							 BOOL fMultiSlice, INT iExecutorSlice)
	: m_spool_id(ulSpoolId),
	  m_spool_type(edxlspstype),
	  m_is_multi_slice_shared(fMultiSlice),
	  m_executor_slice_id(iExecutorSlice)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLSpoolInfo::GetSpoolId
//
//	@doc:
//		Spool id
//
//---------------------------------------------------------------------------
ULONG
CDXLSpoolInfo::GetSpoolId() const
{
	return m_spool_id;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLSpoolInfo::GetSpoolType
//
//	@doc:
//		Type of the underlying operator (Materialize or Sort)
//
//---------------------------------------------------------------------------
Edxlspooltype
CDXLSpoolInfo::GetSpoolType() const
{
	return m_spool_type;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLSpoolInfo::IsMultiSlice
//
//	@doc:
//		Is the spool used across slices
//
//---------------------------------------------------------------------------
BOOL
CDXLSpoolInfo::IsMultiSlice() const
{
	return m_is_multi_slice_shared;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLSpoolInfo::GetExecutorSliceId
//
//	@doc:
//		Id of slice executing the underlying operation
//
//---------------------------------------------------------------------------
INT
CDXLSpoolInfo::GetExecutorSliceId() const
{
	return m_executor_slice_id;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLSpoolInfo::GetSpoolTypeName
//
//	@doc:
//		Id of slice executing the underlying operation
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLSpoolInfo::GetSpoolTypeName() const
{
	GPOS_ASSERT(EdxlspoolMaterialize == m_spool_type ||
				EdxlspoolSort == m_spool_type);

	switch (m_spool_type)
	{
		case EdxlspoolMaterialize:
			return CDXLTokens::GetDXLTokenStr(EdxltokenSpoolMaterialize);
		case EdxlspoolSort:
			return CDXLTokens::GetDXLTokenStr(EdxltokenSpoolSort);
		default:
			return CDXLTokens::GetDXLTokenStr(EdxltokenUnknown);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLSpoolInfo::SerializeToDXL
//
//	@doc:
//		Serialize spooling info in DXL format
//
//---------------------------------------------------------------------------
void
CDXLSpoolInfo::SerializeToDXL(CXMLSerializer *xml_serializer) const
{
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenSpoolId),
								 m_spool_id);

	const CWStringConst *pstrSpoolType = GetSpoolTypeName();
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenSpoolType),
								 pstrSpoolType);

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenSpoolMultiSlice),
		m_is_multi_slice_shared);

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenExecutorSliceId),
		m_executor_slice_id);
}



// EOF

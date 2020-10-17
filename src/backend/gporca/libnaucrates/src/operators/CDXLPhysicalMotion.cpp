//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalMotion.cpp
//
//	@doc:
//		Implementation of DXL physical motion operators
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLPhysicalMotion.h"
#include "naucrates/dxl/operators/CDXLNode.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalMotion::CDXLPhysicalMotion
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLPhysicalMotion::CDXLPhysicalMotion(CMemoryPool *mp)
	: CDXLPhysical(mp), m_input_segids_array(NULL), m_output_segids_array(NULL)
{
}

CDXLPhysicalMotion::~CDXLPhysicalMotion()
{
	CRefCount::SafeRelease(m_input_segids_array);
	CRefCount::SafeRelease(m_output_segids_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalMotion::GetInputSegIdsArray
//
//	@doc:
//
//
//---------------------------------------------------------------------------
const IntPtrArray *
CDXLPhysicalMotion::GetInputSegIdsArray() const
{
	return m_input_segids_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalMotion::GetOutputSegIdsArray
//
//	@doc:
//
//
//---------------------------------------------------------------------------
const IntPtrArray *
CDXLPhysicalMotion::GetOutputSegIdsArray() const
{
	return m_output_segids_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalMotion::SetInputSegIds
//
//	@doc:
//
//
//---------------------------------------------------------------------------
void
CDXLPhysicalMotion::SetInputSegIds(IntPtrArray *input_segids_array)
{
	GPOS_ASSERT(NULL == m_input_segids_array);
	GPOS_ASSERT(NULL != input_segids_array);
	m_input_segids_array = input_segids_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalMotion::SetOutputSegIds
//
//	@doc:
//
//
//---------------------------------------------------------------------------
void
CDXLPhysicalMotion::SetOutputSegIds(IntPtrArray *output_segids_array)
{
	GPOS_ASSERT(NULL == m_output_segids_array);
	GPOS_ASSERT(NULL != output_segids_array);
	m_output_segids_array = output_segids_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalMotion::SetSegmentInfo
//
//	@doc:
//		Set input and output segment information
//
//---------------------------------------------------------------------------
void
CDXLPhysicalMotion::SetSegmentInfo(IntPtrArray *input_segids_array,
								   IntPtrArray *output_segids_array)
{
	GPOS_ASSERT(NULL == m_output_segids_array && NULL == m_input_segids_array);
	GPOS_ASSERT(NULL != output_segids_array && NULL != input_segids_array);

	m_input_segids_array = input_segids_array;
	m_output_segids_array = output_segids_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalMotion::GetSegIdsCommaSeparatedStr
//
//	@doc:
//		Serialize the array of segment ids into a comma-separated string
//
//---------------------------------------------------------------------------
CWStringDynamic *
CDXLPhysicalMotion::GetSegIdsCommaSeparatedStr(
	const IntPtrArray *segment_ids_array) const
{
	GPOS_ASSERT(segment_ids_array != NULL && 0 < segment_ids_array->Size());

	CWStringDynamic *str = GPOS_NEW(m_mp) CWStringDynamic(m_mp);

	ULONG num_of_segments = segment_ids_array->Size();
	for (ULONG idx = 0; idx < num_of_segments; idx++)
	{
		INT segment_id = *((*segment_ids_array)[idx]);
		if (idx == num_of_segments - 1)
		{
			// last element: do not print a comma
			str->AppendFormat(GPOS_WSZ_LIT("%d"), segment_id);
		}
		else
		{
			str->AppendFormat(
				GPOS_WSZ_LIT("%d%ls"), segment_id,
				CDXLTokens::GetDXLTokenStr(EdxltokenComma)->GetBuffer());
		}
	}

	return str;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalMotion::GetInputSegIdsStr
//
//	@doc:
//		Serialize the array of input segment ids into a comma-separated string
//
//---------------------------------------------------------------------------
CWStringDynamic *
CDXLPhysicalMotion::GetInputSegIdsStr() const
{
	return GetSegIdsCommaSeparatedStr(m_input_segids_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalMotion::GetOutputSegIdsStr
//
//	@doc:
//		Serialize the array of output segment ids into a comma-separated string
//
//---------------------------------------------------------------------------
CWStringDynamic *
CDXLPhysicalMotion::GetOutputSegIdsStr() const
{
	return GetSegIdsCommaSeparatedStr(m_output_segids_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalMotion::SerializeSegmentInfoToDXL
//
//	@doc:
//		Serialize the array of input and output segment ids in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalMotion::SerializeSegmentInfoToDXL(
	CXMLSerializer *xml_serializer) const
{
	CWStringDynamic *input_segment_ids_str = GetInputSegIdsStr();
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenInputSegments),
		input_segment_ids_str);

	CWStringDynamic *output_segment_ids_str = GetOutputSegIdsStr();
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenOutputSegments),
		output_segment_ids_str);

	GPOS_DELETE(input_segment_ids_str);
	GPOS_DELETE(output_segment_ids_str);
}


// EOF

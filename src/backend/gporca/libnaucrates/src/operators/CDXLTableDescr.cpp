//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLTableDescr.cpp
//
//	@doc:
//		Implementation of DXL table descriptors
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLTableDescr.h"

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

#define GPDXL_DEFAULT_USERID 0
#define GPDXL_INVALID_LOCKMODE -1

//---------------------------------------------------------------------------
//	@function:
//		CDXLTableDescr::CDXLTableDescr
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLTableDescr::CDXLTableDescr(CMemoryPool *mp, IMDId *mdid, CMDName *mdname,
							   ULONG ulExecuteAsUser, int lockmode,
							   ULONG assigned_query_id_for_target_rel)
	: m_mdid(mdid),
	  m_mdname(mdname),
	  m_dxl_column_descr_array(nullptr),
	  m_execute_as_user_id(ulExecuteAsUser),
	  m_lockmode(lockmode),
	  m_assigned_query_id_for_target_rel(assigned_query_id_for_target_rel)
{
	GPOS_ASSERT(nullptr != m_mdname);
	m_dxl_column_descr_array = GPOS_NEW(mp) CDXLColDescrArray(mp);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLTableDescr::~CDXLTableDescr
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLTableDescr::~CDXLTableDescr()
{
	m_mdid->Release();
	GPOS_DELETE(m_mdname);
	CRefCount::SafeRelease(m_dxl_column_descr_array);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLTableDescr::MDId
//
//	@doc:
//		Return the metadata id for the table
//
//---------------------------------------------------------------------------
IMDId *
CDXLTableDescr::MDId() const
{
	return m_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTableDescr::MdName
//
//	@doc:
//		Return table name
//
//---------------------------------------------------------------------------
const CMDName *
CDXLTableDescr::MdName() const
{
	return m_mdname;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTableDescr::Arity
//
//	@doc:
//		Return number of columns in the table
//
//---------------------------------------------------------------------------
ULONG
CDXLTableDescr::Arity() const
{
	return (m_dxl_column_descr_array == nullptr)
			   ? 0
			   : m_dxl_column_descr_array->Size();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTableDescr::GetExecuteAsUserId
//
//	@doc:
//		Id of the user the table needs to be accessed with
//
//---------------------------------------------------------------------------
ULONG
CDXLTableDescr::GetExecuteAsUserId() const
{
	return m_execute_as_user_id;
}

INT
CDXLTableDescr::LockMode() const
{
	return m_lockmode;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTableDescr::SetColumnDescriptors
//
//	@doc:
//		Set the list of column descriptors
//
//---------------------------------------------------------------------------
void
CDXLTableDescr::SetColumnDescriptors(CDXLColDescrArray *dxl_column_descr_array)
{
	CRefCount::SafeRelease(m_dxl_column_descr_array);
	m_dxl_column_descr_array = dxl_column_descr_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTableDescr::AddColumnDescr
//
//	@doc:
//		Add a column to the list of column descriptors
//
//---------------------------------------------------------------------------
void
CDXLTableDescr::AddColumnDescr(CDXLColDescr *column_descr_dxl)
{
	GPOS_ASSERT(nullptr != m_dxl_column_descr_array);
	GPOS_ASSERT(nullptr != column_descr_dxl);
	m_dxl_column_descr_array->Append(column_descr_dxl);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTableDescr::GetColumnDescrAt
//
//	@doc:
//		Get the column descriptor at the specified position from the col descr list
//
//---------------------------------------------------------------------------
const CDXLColDescr *
CDXLTableDescr::GetColumnDescrAt(ULONG idx) const
{
	GPOS_ASSERT(idx < Arity());

	return (*m_dxl_column_descr_array)[idx];
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTableDescr::SerializeMDId
//
//	@doc:
//		Serialize the metadata id in DXL format
//
//---------------------------------------------------------------------------
void
CDXLTableDescr::SerializeMDId(CXMLSerializer *xml_serializer) const
{
	m_mdid->Serialize(xml_serializer,
					  CDXLTokens::GetDXLTokenStr(EdxltokenMdid));
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLTableDescr::SerializeToDXL
//
//	@doc:
//		Serialize table descriptor in DXL format
//
//---------------------------------------------------------------------------
void
CDXLTableDescr::SerializeToDXL(CXMLSerializer *xml_serializer) const
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenTableDescr));

	SerializeMDId(xml_serializer);

	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenTableName),
								 m_mdname->GetMDName());

	if (GPDXL_DEFAULT_USERID != m_execute_as_user_id)
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenExecuteAsUser),
			m_execute_as_user_id);
	}

	if (GPDXL_INVALID_LOCKMODE != LockMode())
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenLockMode), LockMode());
	}

	if (UNASSIGNED_QUERYID != m_assigned_query_id_for_target_rel)
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenAssignedQueryIdForTargetRel),
			m_assigned_query_id_for_target_rel);
	}

	// serialize columns
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenColumns));
	GPOS_ASSERT(nullptr != m_dxl_column_descr_array);

	const ULONG arity = Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CDXLColDescr *pdxlcd = (*m_dxl_column_descr_array)[ul];
		pdxlcd->SerializeToDXL(xml_serializer);
	}

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenColumns));

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenTableDescr));
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLTableDescr::GetAssignedQueryIdForTargetRel
//
//	@doc:
//		Return id of query, to which TableDescr belongs to
//		(if this descriptor points to a result (target) entry,
//		else UNASSIGNED_QUERYID returned)
//
//---------------------------------------------------------------------------
ULONG
CDXLTableDescr::GetAssignedQueryIdForTargetRel() const
{
	return m_assigned_query_id_for_target_rel;
}

// EOF

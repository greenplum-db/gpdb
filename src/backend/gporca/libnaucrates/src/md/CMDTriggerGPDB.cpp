//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CMDTriggerGPDB.cpp
//
//	@doc:
//		Implementation of the class for representing GPDB-specific triggers
//		in the MD cache
//---------------------------------------------------------------------------


#include "naucrates/md/CMDTriggerGPDB.h"

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpmd;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CMDTriggerGPDB::CMDTriggerGPDB
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CMDTriggerGPDB::CMDTriggerGPDB(CMemoryPool *mp, IMDId *mdid, CMDName *mdname,
							   IMDId *rel_mdid, IMDId *mdid_func, INT type,
							   BOOL is_enabled)
	: m_mp(mp),
	  m_mdid(mdid),
	  m_mdname(mdname),
	  m_rel_mdid(rel_mdid),
	  m_func_mdid(mdid_func),
	  m_type(type),
	  m_is_enabled(is_enabled)
{
	GPOS_ASSERT(m_mdid->IsValid());
	GPOS_ASSERT(m_rel_mdid->IsValid());
	GPOS_ASSERT(m_func_mdid->IsValid());
	GPOS_ASSERT(0 <= type);

	m_dxl_str = CDXLUtils::SerializeMDObj(
		m_mp, this, false /*fSerializeHeader*/, false /*indentation*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTriggerGPDB::~CMDTriggerGPDB
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CMDTriggerGPDB::~CMDTriggerGPDB()
{
	m_mdid->Release();
	m_rel_mdid->Release();
	m_func_mdid->Release();
	GPOS_DELETE(m_mdname);
	GPOS_DELETE(m_dxl_str);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTriggerGPDB::FRow
//
//	@doc:
//		Does trigger execute on a row-level
//
//---------------------------------------------------------------------------
BOOL
CMDTriggerGPDB::ExecutesOnRowLevel() const
{
	return (m_type & GPMD_TRIGGER_ROW);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTriggerGPDB::IsBefore
//
//	@doc:
//		Is this a before trigger
//
//---------------------------------------------------------------------------
BOOL
CMDTriggerGPDB::IsBefore() const
{
	return (m_type & GPMD_TRIGGER_BEFORE);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTriggerGPDB::Insert
//
//	@doc:
//		Is this an insert trigger
//
//---------------------------------------------------------------------------
BOOL
CMDTriggerGPDB::IsInsert() const
{
	return (m_type & GPMD_TRIGGER_INSERT);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTriggerGPDB::FDelete
//
//	@doc:
//		Is this a delete trigger
//
//---------------------------------------------------------------------------
BOOL
CMDTriggerGPDB::IsDelete() const
{
	return (m_type & GPMD_TRIGGER_DELETE);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTriggerGPDB::FUpdate
//
//	@doc:
//		Is this an update trigger
//
//---------------------------------------------------------------------------
BOOL
CMDTriggerGPDB::IsUpdate() const
{
	return (m_type & GPMD_TRIGGER_UPDATE);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTriggerGPDB::Serialize
//
//	@doc:
//		Serialize trigger metadata in DXL format
//
//---------------------------------------------------------------------------
void
CMDTriggerGPDB::Serialize(CXMLSerializer *xml_serializer) const
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBTrigger));

	m_mdid->Serialize(xml_serializer,
					  CDXLTokens::GetDXLTokenStr(EdxltokenMdid));
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenName),
								 m_mdname->GetMDName());
	m_rel_mdid->Serialize(xml_serializer,
						  CDXLTokens::GetDXLTokenStr(EdxltokenRelationMdid));
	m_func_mdid->Serialize(xml_serializer,
						   CDXLTokens::GetDXLTokenStr(EdxltokenFuncId));

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBTriggerRow),
		ExecutesOnRowLevel());
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBTriggerBefore), IsBefore());
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBTriggerInsert), IsInsert());
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBTriggerDelete), IsDelete());
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBTriggerUpdate), IsUpdate());
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBTriggerEnabled), m_is_enabled);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBTrigger));
}

#ifdef GPOS_DEBUG

//---------------------------------------------------------------------------
//	@function:
//		CMDTriggerGPDB::DebugPrint
//
//	@doc:
//		Prints a metadata cache relation to the provided output
//
//---------------------------------------------------------------------------
void
CMDTriggerGPDB::DebugPrint(IOstream &os) const
{
	os << "Trigger id: ";
	m_mdid->OsPrint(os);
	os << std::endl;

	os << "Trigger name: " << (Mdname()).GetMDName()->GetBuffer() << std::endl;

	os << "Trigger relation id: ";
	m_rel_mdid->OsPrint(os);
	os << std::endl;

	os << "Trigger function id: ";
	m_func_mdid->OsPrint(os);
	os << std::endl;

	if (ExecutesOnRowLevel())
	{
		os << "Trigger level: Row" << std::endl;
	}
	else
	{
		os << "Trigger level: Table" << std::endl;
	}

	if (IsBefore())
	{
		os << "Trigger timing: Before" << std::endl;
	}
	else
	{
		os << "Trigger timing: After" << std::endl;
	}

	os << "Trigger statement type(s): [ ";
	if (IsInsert())
	{
		os << "Insert ";
	}

	if (IsDelete())
	{
		os << "Delete ";
	}

	if (IsUpdate())
	{
		os << "Update ";
	}
	os << "]" << std::endl;

	if (m_is_enabled)
	{
		os << "Trigger enabled: Yes" << std::endl;
	}
	else
	{
		os << "Trigger enabled: No" << std::endl;
	}
}

#endif	// GPOS_DEBUG

// EOF

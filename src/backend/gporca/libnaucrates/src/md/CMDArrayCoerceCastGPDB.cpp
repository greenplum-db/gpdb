//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CMDArrayCoerceCastGPDB.cpp
//
//	@doc:
//		Implementation of the class for representing GPDB-specific array coerce
//		casts in the MD cache
//---------------------------------------------------------------------------


#include "naucrates/md/CMDArrayCoerceCastGPDB.h"

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpmd;
using namespace gpdxl;

// ctor
CMDArrayCoerceCastGPDB::CMDArrayCoerceCastGPDB(
	CMemoryPool *mp, IMDId *mdid, CMDName *mdname, IMDId *mdid_src,
	IMDId *mdid_dest, BOOL is_binary_coercible, IMDId *mdid_cast_func,
	EmdCoercepathType path_type, INT type_modifier, BOOL is_explicit,
	EdxlCoercionForm dxl_coerce_format, INT location)
	: CMDCastGPDB(mp, mdid, mdname, mdid_src, mdid_dest, is_binary_coercible,
				  mdid_cast_func, path_type),
	  m_type_modifier(type_modifier),
	  m_is_explicit(is_explicit),
	  m_dxl_coerce_format(dxl_coerce_format),
	  m_location(location)
{
	m_dxl_str = CDXLUtils::SerializeMDObj(mp, this, false /*fSerializeHeader*/,
										  false /*indentation*/);
}

// dtor
CMDArrayCoerceCastGPDB::~CMDArrayCoerceCastGPDB()
{
	GPOS_DELETE(m_dxl_str);
}

// return type modifier
INT
CMDArrayCoerceCastGPDB::TypeModifier() const
{
	return m_type_modifier;
}

// return is explicit cast
BOOL
CMDArrayCoerceCastGPDB::IsExplicit() const
{
	return m_is_explicit;
}

// return coercion form
EdxlCoercionForm
CMDArrayCoerceCastGPDB::GetCoercionForm() const
{
	return m_dxl_coerce_format;
}

// return token location
INT
CMDArrayCoerceCastGPDB::Location() const
{
	return m_location;
}

// serialize function metadata in DXL format
void
CMDArrayCoerceCastGPDB::Serialize(CXMLSerializer *xml_serializer) const
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBArrayCoerceCast));

	m_mdid->Serialize(xml_serializer,
					  CDXLTokens::GetDXLTokenStr(EdxltokenMdid));

	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenName),
								 m_mdname->GetMDName());

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBCastCoercePathType),
		m_path_type);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBCastBinaryCoercible),
		m_is_binary_coercible);

	m_mdid_src->Serialize(xml_serializer,
						  CDXLTokens::GetDXLTokenStr(EdxltokenGPDBCastSrcType));
	m_mdid_dest->Serialize(
		xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenGPDBCastDestType));
	m_mdid_cast_func->Serialize(
		xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenGPDBCastFuncId));

	if (default_type_modifier != TypeModifier())
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenTypeMod), TypeModifier());
	}

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenIsExplicit), m_is_explicit);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenCoercionForm),
		(ULONG) m_dxl_coerce_format);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenLocation),
								 m_location);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBArrayCoerceCast));
}


#ifdef GPOS_DEBUG

// prints a metadata cache relation to the provided output
void
CMDArrayCoerceCastGPDB::DebugPrint(IOstream &os) const
{
	CMDCastGPDB::DebugPrint(os);
	os << ", Result Type Mod: ";
	os << m_type_modifier;
	os << ", isExplicit: ";
	os << m_is_explicit;
	os << ", coercion form: ";
	os << m_dxl_coerce_format;

	os << std::endl;
}

#endif	// GPOS_DEBUG

// EOF

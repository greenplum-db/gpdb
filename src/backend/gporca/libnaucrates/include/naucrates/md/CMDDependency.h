//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CMDDependency.h
//
//	@doc:
//		Class representing MD extended stats depencency.
//
//		The structure mirrors MVDependency in statistics.h. One noticable
//		difference is that that in CMDDependency the attributes are split into
//		"from" and "to" attributes.  Whereas MVDependency it is a single array
//		where the last indexed attnum is infered to be the "to" attribute.
//---------------------------------------------------------------------------



#ifndef GPMD_CMDDependency_H
#define GPMD_CMDDependency_H

#include "gpos/base.h"
#include "gpos/common/CDouble.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

namespace gpmd
{
using namespace gpos;
using namespace gpdxl;


class CMDDependency : public CRefCount
{
private:
	// memory pool
	CMemoryPool *m_mp;

	CDouble m_degree;

	IntPtrArray *m_from_attno;

	INT m_to_attno;

public:
	CMDDependency(CMemoryPool *mp, DOUBLE degree, IntPtrArray *from_attno,
				  INT to_attno)
		: m_mp(mp),
		  m_degree(degree),
		  m_from_attno(from_attno),
		  m_to_attno(to_attno)
	{
	}

	~CMDDependency() override
	{
		m_from_attno->Release();
	}

	CWStringDynamic *
	FromAttnosToStr()
	{
		CWStringDynamic *str = GPOS_NEW(m_mp) CWStringDynamic(m_mp);

		ULONG size = m_from_attno->Size();
		for (ULONG i = 0; i < size - 1; i++)
		{
			str->AppendFormat(GPOS_WSZ_LIT("%d,"), *(*m_from_attno)[i]);
		}
		str->AppendFormat(GPOS_WSZ_LIT("%d"), *(*m_from_attno)[size - 1]);

		return str;
	}

	// serialize metadata dependency in DXL format given a serializer object
	void
	Serialize(gpdxl::CXMLSerializer *xml_serializer)
	{
		xml_serializer->OpenElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			CDXLTokens::GetDXLTokenStr(EdxltokenMVDependency));

		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenDegree), m_degree);
		CWStringDynamic *from_attnos_str = FromAttnosToStr();
		xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenFrom),
									 from_attnos_str);
		GPOS_DELETE(from_attnos_str);
		xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenTo),
									 m_to_attno);

		xml_serializer->CloseElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			CDXLTokens::GetDXLTokenStr(EdxltokenMVDependency));
	}

	IntPtrArray *
	GetFromAttno() const
	{
		return m_from_attno;
	}

	INT
	GetToAttno() const
	{
		return m_to_attno;
	}

	CDouble
	GetDegree() const
	{
		return m_degree;
	}

	ULONG
	GetNAttributes() const

	{
		return m_from_attno->Size() + 1;
	}
};

using CMDDependencyArray = CDynamicPtrArray<CMDDependency, CleanupRelease>;
}  // namespace gpmd



#endif	// !GPMD_CMDDependency_H

// EOF

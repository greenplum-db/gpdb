//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CMDNDistinct.h
//
//	@doc:
//		Class representing MD extended stats multivariate n-distinct.
//
//		The structure mirrors MVNDistinct in statistics.h
//---------------------------------------------------------------------------



#ifndef GPMD_CMDNDistinct_H
#define GPMD_CMDNDistinct_H

#include "gpos/base.h"
#include "gpos/common/CBitSetIter.h"
#include "gpos/common/CDouble.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

namespace gpmd
{
using namespace gpos;
using namespace gpdxl;


class CMDNDistinct : public CRefCount
{
private:
	// memory pool
	CMemoryPool *m_mp;

	CDouble m_ndistnct_value;

	CBitSet *m_attrs;

public:
	CMDNDistinct(CMemoryPool *mp, DOUBLE ndistinct_value, CBitSet *attrs)
		: m_mp(mp), m_ndistnct_value(ndistinct_value), m_attrs(attrs)
	{
	}

	~CMDNDistinct() override
	{
		m_attrs->Release();
	}

	CWStringDynamic *
	AttrsToStr()
	{
		CWStringDynamic *str = GPOS_NEW(m_mp) CWStringDynamic(m_mp);
		CBitSetIter bsiter(*m_attrs);

		ULONG count = m_attrs->Size();
		while (bsiter.Advance())
		{
			if (--count > 0)
			{
				str->AppendFormat(GPOS_WSZ_LIT("%d,"), bsiter.Bit());
			}
			else
			{
				str->AppendFormat(GPOS_WSZ_LIT("%d"), bsiter.Bit());
			}
		}

		return str;
	}

	// serialize metadata dependency in DXL format given a serializer object
	void
	Serialize(gpdxl::CXMLSerializer *xml_serializer)
	{
		xml_serializer->OpenElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			CDXLTokens::GetDXLTokenStr(EdxltokenMVNDistinct));

		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenMVNDistinct), m_ndistnct_value);

		CWStringDynamic *attnos_str = AttrsToStr();
		xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenAttno),
									 attnos_str);
		GPOS_DELETE(attnos_str);

		xml_serializer->CloseElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			CDXLTokens::GetDXLTokenStr(EdxltokenMVNDistinct));
	}

	CBitSet *
	GetAttrs() const
	{
		return m_attrs;
	}

	CDouble
	GetNDistinct() const
	{
		return m_ndistnct_value;
	}
};

using CMDNDistinctArray = CDynamicPtrArray<CMDNDistinct, CleanupRelease>;
}  // namespace gpmd



#endif	// !GPMD_CMDNDistinct_H

// EOF

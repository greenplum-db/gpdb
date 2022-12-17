//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2022 VMware Inc.
//
//	@filename:
//		CMDExtStatInfo.h
//
//	@doc:
//		Class representing MD extended stats metadata
//
//		The structure mirrors StatisticExtInfo in pathnodes.h
//---------------------------------------------------------------------------



#ifndef GPMD_CMDExtStatInfo_H
#define GPMD_CMDExtStatInfo_H

#include "gpos/base.h"
#include "gpos/common/CBitSet.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/gpdb_types.h"
#include "naucrates/md/CMDName.h"

namespace gpdxl
{
class CXMLSerializer;
}

namespace gpmd
{
using namespace gpos;
using namespace gpdxl;


class CMDExtStatInfo : public CRefCount
{
public:
	enum Estattype
	{
		EstatDependencies,
		EstatNDistinct,
		EstatMCV,
		EstatSentinel
	};

private:
	CMemoryPool *m_mp;

	OID m_stat_oid;

	CMDName *m_stat_name;

	Estattype m_kind;

	CBitSet *m_keys;

public:
	CMDExtStatInfo(CMemoryPool *mp, OID stat_oid, CMDName *stat_name,
				   Estattype kind, CBitSet *keys)
		: m_mp(mp),
		  m_stat_oid(stat_oid),
		  m_stat_name(stat_name),
		  m_kind(kind),
		  m_keys(keys)
	{
	}

	~CMDExtStatInfo() override
	{
		GPOS_DELETE(m_stat_name);
		m_keys->Release();
	}

	CWStringDynamic *KeysToStr(CMemoryPool *mp);

	CWStringDynamic *KindToStr(CMemoryPool *mp);

	void Serialize(gpdxl::CXMLSerializer *xml_serializer);

	OID
	GetStatOid() const
	{
		return m_stat_oid;
	}

	Estattype
	GetStatKind() const
	{
		return m_kind;
	}

	CBitSet *
	GetStatKeys() const
	{
		return m_keys;
	}
};


using CMDExtStatInfoArray = CDynamicPtrArray<CMDExtStatInfo, CleanupRelease>;

}  // namespace gpmd



#endif	// !GPMD_CMDExtStatInfo_H

// EOF

//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CDXLExtStatsInfo.h
//
//	@doc:
//		Class representing extended stats metadata
//---------------------------------------------------------------------------



#ifndef GPMD_CDXLExtStatsInfo_H
#define GPMD_CDXLExtStatsInfo_H

#include "gpos/base.h"
#include "gpos/common/CDouble.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/md/CMDExtStatsInfo.h"
#include "naucrates/md/IMDExtStatsInfo.h"

namespace gpdxl
{
class CXMLSerializer;
}

namespace gpmd
{
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@class:
//		CDXLExtStatsInfo
//
//	@doc:
//		Class representing extended stats metadata
//
//---------------------------------------------------------------------------
class CDXLExtStatsInfo : public IMDExtStatsInfo
{
private:
	// memory pool
	CMemoryPool *m_mp;

	// metadata id of the object
	IMDId *m_mdid;

	// table name
	CMDName *m_mdname;

	// DXL string for object
	CWStringDynamic *m_dxl_str;

	CMDExtStatsInfoArray *m_extstats_info_array;

public:
	CDXLExtStatsInfo(const CDXLExtStatsInfo &) = delete;

	CDXLExtStatsInfo(CMemoryPool *mp, IMDId *rel_stats_mdid, CMDName *mdname,
					 CMDExtStatsInfoArray *extstats_info_array);

	~CDXLExtStatsInfo() override;

	// the metadata id
	IMDId *MDId() const override;

	// relation name
	CMDName Mdname() const override;

	// DXL string representation of cache object
	const CWStringDynamic *GetStrRepr() const override;

	// serialize relation stats in DXL format given a serializer object
	void Serialize(gpdxl::CXMLSerializer *) const override;

	CMDExtStatsInfoArray *
	GetExtStatInfoArray() const override
	{
		return m_extstats_info_array;
	}

#ifdef GPOS_DEBUG
	// debug print of the metadata ext stats metadata
	void
	DebugPrint(IOstream &os GPOS_UNUSED) const override
	{
	}
#endif

	// dummy relstats
	static CDXLExtStatsInfo *CreateDXLDummyExtStatsInfo(CMemoryPool *mp,
														IMDId *mdid);
};

}  // namespace gpmd



#endif	// !GPMD_CDXLExtStatsInfo_H

// EOF

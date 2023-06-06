//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLRelStats.h
//
//	@doc:
//		Class representing relation stats
//---------------------------------------------------------------------------



#ifndef GPMD_CDXLRelStats_H
#define GPMD_CDXLRelStats_H

#include "gpos/base.h"
#include "gpos/common/CDouble.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/md/CMDIdRelStats.h"
#include "naucrates/md/IMDRelStats.h"

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
//		CDXLRelStats
//
//	@doc:
//		Class representing relation stats
//
//---------------------------------------------------------------------------
class CDXLRelStats : public IMDRelStats
{
private:
	// memory pool
	CMemoryPool *m_mp;

	// metadata id of the object
	CMDIdRelStats *m_rel_stats_mdid;

	// table name
	CMDName *m_mdname;

	// number of rows
	CDouble m_rows;

	// flag to indicate if input relation is empty
	BOOL m_empty;

	// DXL string for object
	CWStringDynamic *m_dxl_str = nullptr;

	// number of blocks (not always up to-to-date)
	ULONG m_relpages;

	// number of all-visible blocks (not always up-to-date)
	ULONG m_relallvisible;

public:
	CDXLRelStats(const CDXLRelStats &) = delete;

	CDXLRelStats(CMemoryPool *mp, CMDIdRelStats *rel_stats_mdid,
				 CMDName *mdname, CDouble rows, BOOL is_empty, ULONG relpages,
				 ULONG relallvisible);

	~CDXLRelStats() override;

	// the metadata id
	IMDId *MDId() const override;

	// relation name
	CMDName Mdname() const override;

	// DXL string representation of cache object
	const CWStringDynamic *GetStrRepr() override;

	// number of rows
	CDouble Rows() const override;

	// number of blocks (not always up to-to-date)
	ULONG
	RelPages() const override
	{
		return m_relpages;
	}

	// number of all-visible blocks (not always up-to-date)
	ULONG
	RelAllVisible() const override
	{
		return m_relallvisible;
	}

	// is statistics on an empty input
	BOOL
	IsEmpty() const override
	{
		return m_empty;
	}

	// serialize relation stats in DXL format given a serializer object
	void Serialize(gpdxl::CXMLSerializer *) const override;

#ifdef GPOS_DEBUG
	// debug print of the metadata relation
	void DebugPrint(IOstream &os) const override;
#endif

	// dummy relstats
	static CDXLRelStats *CreateDXLDummyRelStats(CMemoryPool *mp, IMDId *mdid);
};

}  // namespace gpmd



#endif	// !GPMD_CDXLRelStats_H

// EOF

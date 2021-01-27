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
	CWStringDynamic *m_dxl_str;

	// private copy ctor
	CDXLRelStats(const CDXLRelStats &);

	// number of blocks (not always up to-to-date)
	ULONG m_relpages;

	// number of all-visible blocks (not always up-to-date)
	ULONG m_relallvisible;

public:
	CDXLRelStats(CMemoryPool *mp, CMDIdRelStats *rel_stats_mdid,
				 CMDName *mdname, CDouble rows, BOOL is_empty, ULONG relpages,
				 ULONG relallvisible);

	virtual ~CDXLRelStats();

	// the metadata id
	virtual IMDId *MDId() const;

	// relation name
	virtual CMDName Mdname() const;

	// DXL string representation of cache object
	virtual const CWStringDynamic *GetStrRepr() const;

	// number of rows
	virtual CDouble Rows() const;

	// number of blocks (not always up to-to-date)
	virtual ULONG
	RelPages() const
	{
		return m_relpages;
	}

	// number of all-visible blocks (not always up-to-date)
	virtual ULONG
	RelAllVisible() const
	{
		return m_relallvisible;
	}

	// is statistics on an empty input
	virtual BOOL
	IsEmpty() const
	{
		return m_empty;
	}

	// serialize relation stats in DXL format given a serializer object
	virtual void Serialize(gpdxl::CXMLSerializer *) const;

#ifdef GPOS_DEBUG
	// debug print of the metadata relation
	virtual void DebugPrint(IOstream &os) const;
#endif

	// dummy relstats
	static CDXLRelStats *CreateDXLDummyRelStats(CMemoryPool *mp, IMDId *mdid);
};

}  // namespace gpmd



#endif	// !GPMD_CDXLRelStats_H

// EOF

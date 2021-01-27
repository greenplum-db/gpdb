//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CAutoMDAccessor.h
//
//	@doc:
//		An auto object encapsulating metadata accessor.
//---------------------------------------------------------------------------



#ifndef GPOPT_CAutoMDAccessor_H
#define GPOPT_CAutoMDAccessor_H

#include "gpos/base.h"

#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/mdcache/CMDCache.h"
#include "naucrates/md/CMDProviderMemory.h"

namespace gpmd
{
class IMDCacheObject;
}
namespace gpopt
{
using namespace gpos;
//---------------------------------------------------------------------------
//	@class:
//		CAutoMDAccessor
//
//	@doc:
//		An auto object encapsulating metadata accessor
//
//---------------------------------------------------------------------------
class CAutoMDAccessor : public CStackObject
{
private:
	// metadata provider
	IMDProvider *m_pimdp;

	// do we own cache object?
	BOOL m_fOwnCache;

	// metadata cache
	CMDAccessor::MDCache *m_pcache;

	// metadata accessor
	CMDAccessor *m_pmda;

	// system id
	CSystemId m_sysid;

	// private copy ctor
	CAutoMDAccessor(const CAutoMDAccessor &);

public:
	// ctor
	CAutoMDAccessor(CMemoryPool *mp, IMDProvider *pmdp, CSystemId sysid)
		: m_pimdp(pmdp), m_fOwnCache(true), m_sysid(sysid)
	{
		GPOS_ASSERT(NULL != pmdp);

		m_pcache =
			CCacheFactory::CreateCache<gpmd::IMDCacheObject *, gpopt::CMDKey *>(
				true /*fUnique*/, 0 /* unlimited cache quota */,
				gpopt::CMDKey::UlHashMDKey, gpopt::CMDKey::FEqualMDKey);
		m_pmda = GPOS_NEW(mp) CMDAccessor(mp, m_pcache, sysid, pmdp);
	}

	// ctor
	CAutoMDAccessor(CMemoryPool *mp, IMDProvider *pmdp, CSystemId sysid,
					CMDAccessor::MDCache *pcache)
		: m_pimdp(pmdp), m_fOwnCache(false), m_pcache(pcache), m_sysid(sysid)
	{
		GPOS_ASSERT(NULL != pmdp);
		GPOS_ASSERT(NULL != pcache);

		m_pmda = GPOS_NEW(mp) CMDAccessor(mp, m_pcache, sysid, pmdp);
	}

	// dtor
	virtual ~CAutoMDAccessor()
	{
		// because of dependencies among class members, cleaning up
		// has to take place in the following order
		GPOS_DELETE(m_pmda);
		if (m_fOwnCache)
		{
			GPOS_DELETE(m_pcache);
		}
	}

	// accessor of cache
	CMDAccessor::MDCache *
	Pcache() const
	{
		return m_pcache;
	}

	// accessor of metadata accessor
	CMDAccessor *
	Pmda() const
	{
		return m_pmda;
	}

	// accessor of metadata provider
	IMDProvider *
	Pimdp() const
	{
		return m_pimdp;
	}

	// accessor of system id
	CSystemId
	Sysid() const
	{
		return m_sysid;
	}
};
}  // namespace gpopt

#endif	// !GPOPT_CAutoMDAccessor_H

// EOF

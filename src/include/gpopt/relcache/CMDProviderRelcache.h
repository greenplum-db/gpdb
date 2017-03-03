//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMDProviderRelcache.h
//
//	@doc:
//		Relcache-based provider of metadata objects.
//
//	@test:
//
//
//---------------------------------------------------------------------------



#ifndef GPMD_CMDProviderRelcache_H
#define GPMD_CMDProviderRelcache_H

#include "gpos/base.h"
#include "gpos/string/CWStringBase.h"

#include "naucrates/md/CSystemId.h"
#include "naucrates/md/IMDId.h"
#include "naucrates/md/IMDProvider.h"

// fwd decl
namespace gpopt
{
	class CMDAccessor;
}

namespace gpmd
{
	using namespace gpos;

	//---------------------------------------------------------------------------
	//	@class:
	//		CMDProviderRelcache
	//
	//	@doc:
	//		Relcache-based provider of metadata objects.
	//
	//---------------------------------------------------------------------------
	class CMDProviderRelcache : public IMDProvider
	{
		private:
			// memory pool
			IMemoryPool *m_pmp;

			// private copy ctor
			CMDProviderRelcache(const CMDProviderRelcache&);

		public:
			// ctor/dtor
			explicit
			CMDProviderRelcache(IMemoryPool *pmp);

			~CMDProviderRelcache()
			{
			}

			// returns the DXL string of the requested metadata object
			virtual
			CWStringBase *PstrObject(IMemoryPool *pmp, CMDAccessor *pmda, IMDId *pmdid) const;

			// return the mdid for the requested type
			virtual
			IMDId *Pmdid
				(
				IMemoryPool *pmp,
				CSystemId sysid,
				IMDType::ETypeInfo eti
				)
				const
			{
				return PmdidTypeGPDB(pmp, sysid, eti);
			}

	};
}



#endif // !GPMD_CMDProviderRelcache_H

// EOF

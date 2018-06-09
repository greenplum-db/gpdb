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
			IMemoryPool *m_mp;

			// private copy ctor
			CMDProviderRelcache(const CMDProviderRelcache&);

		public:
			// ctor/dtor
			explicit
			CMDProviderRelcache(IMemoryPool *mp);

			~CMDProviderRelcache()
			{
			}

			// returns the DXL string of the requested metadata object
			virtual
			CWStringBase *GetMDObjDXLStr(IMemoryPool *mp, CMDAccessor *md_accessor, IMDId *md_id) const;

			// return the mdid for the requested type
			virtual
			IMDId *MDId
				(
				IMemoryPool *mp,
				CSystemId sysid,
				IMDType::ETypeInfo type_info
				)
				const
			{
				return GetGPDBTypeMdid(mp, sysid, type_info);
			}

	};
}



#endif // !GPMD_CMDProviderRelcache_H

// EOF

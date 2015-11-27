//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CGPDBAttInfo.h
//
//	@doc:
//		Class to uniquely identify a column in GPDB
//
//	@test:
//
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CGPDBAttInfo_H
#define GPDXL_CGPDBAttInfo_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"
#include "gpos/utils.h"
#include "naucrates/dxl/gpdb_types.h"

namespace gpdxl
{
	using namespace gpos;

	//---------------------------------------------------------------------------
	//	@class:
	//		CGPDBAttInfo
	//
	//	@doc:
	//		Class to uniquely identify a column in GPDB
	//
	//---------------------------------------------------------------------------
	class CGPDBAttInfo: public CRefCount
	{
		private:

			// query level number
			ULONG m_ulQueryLevel;

			// varno in the rtable
			ULONG m_ulVarNo;

			// attno
			INT	m_iAttNo;

			// copy c'tor
			CGPDBAttInfo(const CGPDBAttInfo&);

		public:
			// ctor
			CGPDBAttInfo(ULONG ulQueryLevel, ULONG ulVarNo, INT iAttNo)
				: m_ulQueryLevel(ulQueryLevel), m_ulVarNo(ulVarNo), m_iAttNo(iAttNo)
			{}

			// d'tor
			virtual
			~CGPDBAttInfo() {}

			// accessor
			ULONG UlQueryLevel() const
			{
				return m_ulQueryLevel;
			}

			// accessor
			ULONG UlVarNo() const
			{
				return m_ulVarNo;
			}

			// accessor
			INT IAttNo() const
			{
				return m_iAttNo;
			}

			// equality check
			BOOL FEquals(const CGPDBAttInfo& gpdbattinfo) const
			{
				return m_ulQueryLevel == gpdbattinfo.m_ulQueryLevel
						&& m_ulVarNo == gpdbattinfo.m_ulVarNo
						&& m_iAttNo == gpdbattinfo.m_iAttNo;
			}

			// hash value
			ULONG UlHash() const
			{
				return gpos::UlCombineHashes(
						gpos::UlHash(&m_ulQueryLevel),
						gpos::UlCombineHashes(gpos::UlHash(&m_ulVarNo),
								gpos::UlHash(&m_iAttNo)));
			}
	};

	// hash function
	inline ULONG UlHashGPDBAttInfo
		(
		const CGPDBAttInfo *pgpdbattinfo
		)
	{
		GPOS_ASSERT(NULL != pgpdbattinfo);
		return pgpdbattinfo->UlHash();
	}

	// equality function
	inline BOOL FEqualGPDBAttInfo
		(
		const CGPDBAttInfo *pgpdbattinfoA,
		const CGPDBAttInfo *pgpdbattinfoB
		)
	{
		GPOS_ASSERT(NULL != pgpdbattinfoA && NULL != pgpdbattinfoB);
		return pgpdbattinfoA->FEquals(*pgpdbattinfoB);
	}

}

#endif // !GPDXL_CGPDBAttInfo_H

// EOF

//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CStatsPredJoin.h
//
//	@doc:
//		Join predicate used for join cardinality estimation
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CStatsPredJoin_H
#define GPNAUCRATES_CStatsPredJoin_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"
#include "gpos/common/CDynamicPtrArray.h"
#include "naucrates/statistics/CStatsPred.h"
#include "naucrates/md/IMDType.h"

namespace gpnaucrates
{
	using namespace gpos;
	using namespace gpmd;

	//---------------------------------------------------------------------------
	//	@class:
	//		CStatsPredJoin
	//
	//	@doc:
	//		Join predicate used for join cardinality estimation
	//---------------------------------------------------------------------------
	class CStatsPredJoin : public CRefCount
	{
		private:

			// private copy ctor
			CStatsPredJoin(const CStatsPredJoin &);

			// private assignment operator
			CStatsPredJoin& operator=(CStatsPredJoin &);

			// column id
			ULONG m_colidOuter;

			// comparison type
			CStatsPred::EStatsCmpType m_stats_cmp_type;

			// column id
			ULONG m_colidInner;

		public:

			// c'tor
			CStatsPredJoin
				(
				ULONG colid1,
				CStatsPred::EStatsCmpType stats_cmp_type,
				ULONG colid2
				)
				:
				m_colidOuter(colid1),
				m_stats_cmp_type(stats_cmp_type),
				m_colidInner(colid2)
			{}

			// accessors
			ULONG ColIdOuter() const
			{
				return m_colidOuter;
			}

			// comparison type
			CStatsPred::EStatsCmpType GetCmpType() const
			{
				return m_stats_cmp_type;
			}

			ULONG ColIdInner() const
			{
				return m_colidInner;
			}

			// d'tor
			virtual
			~CStatsPredJoin()
			{}

	}; // class CStatsPredJoin

	// array of filters
	typedef CDynamicPtrArray<CStatsPredJoin, CleanupRelease> CStatsPredJoinArray;
}  // namespace gpnaucrates

#endif // !GPNAUCRATES_CStatsPredJoin_H

// EOF

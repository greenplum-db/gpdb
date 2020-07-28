//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2020 VMware, Inc.
//
//	Transform Left Outer Join with a GbAgg on the inner branch to
//	Btree IndexGet Apply
//
//---------------------------------------------------------------------------

#ifndef GPOPT_CXformLeftOuterJoinWithInnerGbAgg2IndexGetApply_H
#define GPOPT_CXformLeftOuterJoinWithInnerGbAgg2IndexGetApply_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformJoin2IndexApplyBase.h"

namespace gpopt
{
	using namespace gpos;

	class CXformLeftOuterJoinWithInnerGbAgg2IndexGetApply : public CXformJoin2IndexApplyBase
		<CLogicalLeftOuterJoin, CLogicalIndexApply, CLogicalGet,
		false /*fWithSelect*/, true /*fWithGbAgg*/, false /*is_partial*/, IMDIndex::EmdindBtree>
	{
		private:
			// private copy ctor
			CXformLeftOuterJoinWithInnerGbAgg2IndexGetApply
				(
				const CXformLeftOuterJoinWithInnerGbAgg2IndexGetApply &
				);

		public:
			// ctor
			explicit
			CXformLeftOuterJoinWithInnerGbAgg2IndexGetApply(CMemoryPool *mp)
				: CXformJoin2IndexApplyBase
				<CLogicalLeftOuterJoin, CLogicalIndexApply, CLogicalGet,
				false /*fWithSelect*/, true /*fWithGbAgg*/, false /*is_partial*/, IMDIndex::EmdindBtree>
				(mp)
			{}

			// dtor
			virtual
			~CXformLeftOuterJoinWithInnerGbAgg2IndexGetApply()
			{}

			// ident accessors
			virtual
			EXformId Exfid() const
			{
				return ExfLeftOuterJoinWithInnerGbAgg2IndexGetApply;
			}

			virtual
			const CHAR *SzId() const
			{
				return "CXformLeftOuterJoinWithInnerGbAgg2IndexGetApply";
			}
	}; // class CXformLeftOuterJoinWithInnerGbAgg2IndexGetApply
}

#endif // !GPOPT_CXformLeftOuterJoinWithInnerGbAgg2IndexGetApply_H

// EOF

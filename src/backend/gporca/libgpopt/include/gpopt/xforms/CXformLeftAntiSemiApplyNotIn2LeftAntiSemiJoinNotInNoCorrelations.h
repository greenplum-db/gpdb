//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations.h
//
//	@doc:
//		Turn LS apply into LS join (NotIn semantics) when inner child has no
//		outer references
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations_H
#define GPOPT_CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformApply2Join.h"
#include "gpopt/operators/CLogicalLeftAntiSemiApplyNotIn.h"
#include "gpopt/operators/CLogicalLeftAntiSemiJoinNotIn.h"


namespace gpopt
{
	using namespace gpos;

	//---------------------------------------------------------------------------
	//	@class:
	//		CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations
	//
	//	@doc:
	//		Transform Apply into Join by decorrelating the inner side
	//
	//---------------------------------------------------------------------------
	class CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations :
		public CXformApply2Join<CLogicalLeftAntiSemiApplyNotIn, CLogicalLeftAntiSemiJoinNotIn>
	{

		private:

			// private copy ctor
			CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations
				(
				const CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations &
				);

		public:

			// ctor
			explicit
			CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations
				(
				CMemoryPool *mp
				)
				:
				CXformApply2Join<CLogicalLeftAntiSemiApplyNotIn, CLogicalLeftAntiSemiJoinNotIn>(mp)
			{}

			// dtor
			virtual
			~CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations()
			{}

			// ident accessors
			virtual
			EXformId Exfid() const
			{
				return ExfLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations;
			}

			// xform name
			virtual
			const CHAR *SzId() const
			{
				return "CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations";
			}

			// compute xform promise for a given expression handle
			virtual
			EXformPromise Exfp(CExpressionHandle &exprhdl) const;

			// actual transform
			void Transform(CXformContext *pxfctxt, CXformResult *pxfres, CExpression *pexpr) const;

	}; // class CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations

}

#endif // !GPOPT_CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations_H

// EOF

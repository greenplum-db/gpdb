//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal Inc.
//
//	@filename:
//		CWindowPreprocessor.h
//
//	@doc:
//		Preprocessing routines of window functions
//---------------------------------------------------------------------------
#ifndef GPOPT_CWindowPreprocessor_H
#define GPOPT_CWindowPreprocessor_H

#include "gpos/base.h"
#include "gpopt/base/CUtils.h"

#include "gpopt/operators/CExpression.h"

namespace gpopt
{

	//---------------------------------------------------------------------------
	//	@class:
	//		CWindowPreprocessor
	//
	//	@doc:
	//		Preprocessing routines of window functions
	//
	//---------------------------------------------------------------------------
	class CWindowPreprocessor
	{
		private:

			// private copy ctor
			CWindowPreprocessor(const CWindowPreprocessor &);

			// iterate over project elements and split them elements between Distinct Aggs list, and Others list
			static
			void SplitPrjList
				(
				CMemoryPool *mp,
				CExpression *pexprSeqPrj,
				CExpressionArray **ppdrgpexprDistinctAggsPrjElems,
				CExpressionArray **ppdrgpexprOtherPrjElems,
				COrderSpecArray **ppdrgposOther,
				CWindowFrameArray **ppdrgpwfOther
				);

			// split given SeqPrj expression into:
			//	- A GbAgg expression containing distinct Aggs, and
			//	- A SeqPrj expression containing all other window functions
			static
			void SplitSeqPrj
				(
				CMemoryPool *mp,
				CExpression *pexprSeqPrj,
				CExpression **ppexprGbAgg,
				CExpression **ppexprOutputSeqPrj
				);

			// create a CTE with two consumers using the child expression of Sequence Project
			static
			void CreateCTE
				(
				CMemoryPool *mp,
				CExpression *pexprSeqPrj,
				CExpression **ppexprFirstConsumer,
				CExpression **ppexprSecondConsumer
				);

			// extract grouping columns from given expression
			static
			CColRefArray *PdrgpcrGrpCols(CExpression *pexprJoinDQAs);

			// transform sequence project expression into an inner join expression
			static
			CExpression *PexprSeqPrj2Join(CMemoryPool *mp, CExpression *pexprSeqPrj);

		public:

			// main driver
			static
			CExpression *PexprPreprocess(CMemoryPool *mp, CExpression *pexpr);

	}; // class CWindowPreprocessor
}


#endif // !GPOPT_CWindowPreprocessor_H

// EOF

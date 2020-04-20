//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.

#include "gpopt/operators/CStrictHashedDistributions.h"
#include "gpopt/base/CDistributionSpecStrictRandom.h"
#include "gpopt/base/CUtils.h"

using namespace gpopt;

CStrictHashedDistributions::CStrictHashedDistributions
(
CMemoryPool *mp,
CColRefArray *pdrgpcrOutput,
CColRef2dArray *pdrgpdrgpcrInput
)
:
CDistributionSpecArray(mp)
{
	const ULONG num_cols = pdrgpcrOutput->Size();
	const ULONG arity = pdrgpdrgpcrInput->Size();
	for (ULONG ulChild = 0; ulChild < arity; ulChild++)
	{
		CColRefArray *colref_array = (*pdrgpdrgpcrInput)[ulChild];
		CExpressionArray *pdrgpexpr = GPOS_NEW(mp) CExpressionArray(mp);
		for (ULONG ulCol = 0; ulCol < num_cols; ulCol++)
		{
			CColRef *colref = (*colref_array)[ulCol];
			if (colref->RetrieveType()->IsRedistributable())
			{
				CExpression *pexpr = CUtils::PexprScalarIdent(mp, colref);
				pdrgpexpr->Append(pexpr);
			}
		}

		CDistributionSpec *pdshashed;
		ULONG ulColumnsToRedistribute = pdrgpexpr->Size();
		if (0 < ulColumnsToRedistribute)
		{
			// create a hashed distribution on input columns of the current child
			BOOL fNullsColocated = true;
			pdshashed = GPOS_NEW(mp) CDistributionSpecStrictHashed(pdrgpexpr, fNullsColocated);
		}
		else
		{
			// None of the input columns are redistributable, but we want to
			// parallelize the relations we are concatenating, so we generate
			// a random redistribution.
			// When given a plan containing a "hash" redistribution on _no_ columns,
			// Some databases actually execute it as if it's a random redistribution.
			// We should not generate such a plan, for clarity and our own sanity

			pdshashed = GPOS_NEW(mp) CDistributionSpecStrictRandom();
			pdrgpexpr->Release();
		}
		Append(pdshashed);
	}
}

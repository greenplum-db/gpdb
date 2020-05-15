//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.

#ifndef GPOPT_CHashedDistributions_H
#define GPOPT_CHashedDistributions_H

#include "gpopt/base/CDistributionSpec.h"
#include "gpopt/base/CDistributionSpecStrictHashed.h"
#include "gpos/memory/CMemoryPool.h"
#include "gpopt/base/CColRef.h"

namespace gpopt
{
	// Build hashed distributions used in physical union all during
	// distribution derivation. The class is an array of hashed
	// distribution on input column of each child, and an output hashed
	// distribution on UnionAll output columns
	// If there exists no redistributable columns in the input list,
	// it creates a random distribution.
	class CStrictHashedDistributions : public CDistributionSpecArray
	{
		public:
			CStrictHashedDistributions
			(
			CMemoryPool *mp,
			CColRefArray *pdrgpcrOutput,
			CColRef2dArray *pdrgpdrgpcrInput
			);
	};
}

#endif //GPOPT_CHashedDistributions_H

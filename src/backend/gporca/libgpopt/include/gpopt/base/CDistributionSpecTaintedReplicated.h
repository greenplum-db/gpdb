//	Greenplum Database
//	Copyright (C) 2020 VMware Inc.

#ifndef GPOPT_CDistributionSpecTaintedReplicated_H
#define GPOPT_CDistributionSpecTaintedReplicated_H

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpec.h"

namespace gpopt
{

	// derive-only: unsafe computation over replicated data
	class CDistributionSpecTaintedReplicated : public CDistributionSpec
	{
		public:
			// ctor
			CDistributionSpecTaintedReplicated() = default;

			// accessor
			virtual 
			EDistributionType Edt() const
			{
				return CDistributionSpec::EdtTaintedReplicated;
			}

			// does this distribution satisfy the given one
			virtual 
			BOOL FSatisfies(const CDistributionSpec *pds) const;

			// should never be called on a derive-only distribution
			virtual
			void AppendEnforcers(gpos::CMemoryPool *mp, CExpressionHandle &exprhdl, CReqdPropPlan *prpp, CExpressionArray *pdrgpexpr, CExpression *pexpr);

			// return distribution partitioning type
			virtual
			EDistributionPartitioningType Edpt() const
			{
				return EdptNonPartitioned;
			}

			// print
			virtual
			IOstream &OsPrint(IOstream &os) const
			{
				return os << "TAINTED REPLICATED ";
			}
			
			// conversion function
			static
			CDistributionSpecTaintedReplicated *PdsConvert
				(
				CDistributionSpec *pds
				)
			{
				GPOS_ASSERT(NULL != pds);
				GPOS_ASSERT(EdtTaintedReplicated == pds->Edt());

				return dynamic_cast<CDistributionSpecTaintedReplicated*>(pds);
			}
	};
	// class CDistributionSpecTaintedReplicated

}

#endif // !GPOPT_CDistributionSpecTaintedReplicated_H

//	Greenplum Database
//	Copyright (C) 2020 VMware Inc.

#ifndef GPOPT_CDistributionSpecGeneralReplicated_H
#define GPOPT_CDistributionSpecGeneralReplicated_H

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpec.h"

namespace gpopt
{

	// derive-only: unsafe computation over replicated data
	class CDistributionSpecGeneralReplicated : public CDistributionSpec
	{
		public:
			// ctor
			CDistributionSpecGeneralReplicated() = default;

			// accessor
			virtual
			EDistributionType Edt() const
			{
				return CDistributionSpec::EdtGeneralReplicated;
			}

			// should never be called on a required-only distribution
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
				return os << "GENERAL REPLICATED";
			}

			// conversion function
			static
			CDistributionSpecGeneralReplicated *PdsConvert
				(
				CDistributionSpec *pds
				)
			{
				GPOS_ASSERT(NULL != pds);
				GPOS_ASSERT(EdtGeneralReplicated == pds->Edt());

				return dynamic_cast<CDistributionSpecGeneralReplicated*>(pds);
			}
	};
	// class CDistributionSpecGeneralReplicated

}

#endif // !GPOPT_CDistributionSpecGeneralReplicated_H

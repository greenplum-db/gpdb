//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDistributionSpecAny.h
//
//	@doc:
//		Description of a general distribution which imposes no requirements; 
//		Can be used only as a required property;
//---------------------------------------------------------------------------
#ifndef GPOPT_CDistributionSpecAny_H
#define GPOPT_CDistributionSpecAny_H

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpec.h"
#include "gpopt/operators/COperator.h"

namespace gpopt
{
	using namespace gpos;

	//---------------------------------------------------------------------------
	//	@class:
	//		CDistributionSpecAny
	//
	//	@doc:
	//		Class for representing general distribution specification which 
	//		imposes no requirements.
	//
	//---------------------------------------------------------------------------
	class CDistributionSpecAny : public CDistributionSpec
	{
		private:

			// the physical operator that originally requested this distribution spec
			COperator::EOperatorId m_eopidRequested;

			// allow outer references in the operator tree where distribution is requested
			BOOL m_fAllowOuterRefs;

			// private copy ctor
			CDistributionSpecAny(const CDistributionSpecAny &);
			

		public:

			//ctor
			CDistributionSpecAny
				(
				COperator::EOperatorId eopidRequested
				)
				:
				m_eopidRequested(eopidRequested),
				m_fAllowOuterRefs(false)
			{}
			
			//ctor
			CDistributionSpecAny
				(
				COperator::EOperatorId eopidRequested,
				BOOL fAllowOuterRefs
				)
				:
				m_eopidRequested(eopidRequested),
				m_fAllowOuterRefs(fAllowOuterRefs)
			{}

			// accessor
			virtual 
			EDistributionType Edt() const
			{
				return CDistributionSpec::EdtAny;
			}
			
			// does current distribution satisfy the given one
			virtual 
			BOOL FSatisfies(const CDistributionSpec *pds) const
			{
				return EdtAny == pds->Edt();
			}
			
			// return true if distribution spec can be derived
			virtual
			BOOL FDerivable() const
			{
				return false;
			}

			// append enforcers to dynamic array for the given plan properties
			virtual
			void AppendEnforcers
				(
				CMemoryPool * , // mp 
				CExpressionHandle &, // exprhdl: gives access to child properties
				CReqdPropPlan *, // prpp
				CExpressionArray *, // pdrgpexpr 
				CExpression * // pexpr				
				)
			{
				GPOS_ASSERT(!"attempt to enforce ANY distribution");
			}

			// print
			virtual
			IOstream &OsPrint(IOstream &os) const
			{
				return os << "ANY " << " EOperatorId: " << m_eopidRequested << " ";
			}

			// return distribution partitioning type
			virtual
			EDistributionPartitioningType Edpt() const
			{
				return EdptUnknown;
			}

			// allow outer references in the operator tree where distribution is requested
			BOOL FAllowOuterRefs() const
			{
				return m_fAllowOuterRefs;
			}

			// conversion function
			static
			CDistributionSpecAny *PdsConvert
				(
				CDistributionSpec *pds
				)
			{
				GPOS_ASSERT(NULL != pds);
				GPOS_ASSERT(EdtAny == pds->Edt());

				return dynamic_cast<CDistributionSpecAny*>(pds);
			}

			// retrieve the physical operator requesting this distribution spec
			COperator::EOperatorId
			GetRequestedOperatorId() const
			{
				return m_eopidRequested;
			}

	}; // class CDistributionSpecAny

}

#endif // !GPOPT_CDistributionSpecAny_H

// EOF

//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalDML.h
//
//	@doc:
//		Physical DML operator
//---------------------------------------------------------------------------
#ifndef GPOS_CPhysicalDML_H
#define GPOS_CPhysicalDML_H

#include "gpos/base.h"
#include "gpopt/operators/CPhysical.h"
#include "gpopt/operators/CLogicalDML.h"


namespace gpopt
{
	// fwd declaration
	class CDistributionSpec;
	
	//---------------------------------------------------------------------------
	//	@class:
	//		CPhysicalDML
	//
	//	@doc:
	//		Physical DML operator
	//
	//---------------------------------------------------------------------------
	class CPhysicalDML : public CPhysical
	{
		private:

			// dml operator
			CLogicalDML::EDMLOperator m_edmlop;

			// table descriptor
			CTableDescriptor *m_ptabdesc;
			
			// array of source columns
			CColRefArray *m_pdrgpcrSource;
			
			// set of modified columns from the target table
			CBitSet *m_pbsModified;
	
			// action column
			CColRef *m_pcrAction;

			// table oid column
			CColRef *m_pcrTableOid;

			// ctid column
			CColRef *m_pcrCtid;

			// segmentid column
			CColRef *m_pcrSegmentId;
			
			// tuple oid column
			CColRef *m_pcrTupleOid;

			// target table distribution spec
			CDistributionSpec *m_pds;

			// required order spec
			COrderSpec *m_pos;
	
			// required columns by local members
			CColRefSet *m_pcrsRequiredLocal;

			// needs the data to be sorted or not
			BOOL m_input_sort_req;

			// do we need to sort on parquet table
			BOOL FInsertSortOnParquet();

			// do we need to sort on insert
			BOOL FInsertSortOnRows(COptimizerConfig *optimizer_config);

			// compute required order spec
			COrderSpec *PosComputeRequired
				(
				CMemoryPool *mp, 
				CTableDescriptor *ptabdesc
				);
			
			// compute local required columns
			void ComputeRequiredLocalColumns(CMemoryPool *mp);

			// private copy ctor
			CPhysicalDML(const CPhysicalDML &);

		public:

			// ctor
			CPhysicalDML
				(
				CMemoryPool *mp,
				CLogicalDML::EDMLOperator edmlop,
				CTableDescriptor *ptabdesc,
				CColRefArray *pdrgpcrSource,
				CBitSet *pbsModified,
				CColRef *pcrAction,
				CColRef *pcrTableOid,
				CColRef *pcrCtid,
				CColRef *pcrSegmentId,
				CColRef *pcrTupleOid
				);

			// dtor
			virtual
			~CPhysicalDML();

			// ident accessors
			virtual 
			EOperatorId Eopid() const
			{
				return EopPhysicalDML;
			}

			// return a string for operator name
			virtual 
			const CHAR *SzId() const
			{
				return "CPhysicalDML";
			}

			// dml operator
			CLogicalDML::EDMLOperator Edmlop() const
			{
				return m_edmlop;
			}

			// table descriptor
			CTableDescriptor *Ptabdesc() const
			{
				return m_ptabdesc;
			}

			// action column
			CColRef *PcrAction() const
			{
				return m_pcrAction;
			}

			// table oid column
			CColRef *PcrTableOid() const
			{
				return m_pcrTableOid;
			}

			// ctid column
			CColRef *PcrCtid() const
			{
				return m_pcrCtid;
			}

			// segmentid column
			CColRef *PcrSegmentId() const
			{
				return m_pcrSegmentId;
			}

			// tuple oid column
			CColRef *PcrTupleOid() const
			{
				return m_pcrTupleOid;
			}
			
			// source columns
			virtual
			CColRefArray *PdrgpcrSource() const
			{
				return m_pdrgpcrSource;
			}

			// match function
			virtual
			BOOL Matches(COperator *pop) const;

			// hash function
			virtual
			ULONG HashValue() const;

			// sensitivity to order of inputs
			virtual
			BOOL FInputOrderSensitive() const
			{
				return false;
			}

			// needs the data to be sorted or not
			virtual
			BOOL IsInputSortReq() const
			{
				return m_input_sort_req;
			}
					
			//-------------------------------------------------------------------------------------
			// Required Plan Properties
			//-------------------------------------------------------------------------------------

			// compute required sort columns of the n-th child
			virtual
			COrderSpec *PosRequired
				(
				CMemoryPool *mp,
				CExpressionHandle &exprhdl,
				COrderSpec *posRequired,
				ULONG child_index,
				CDrvdPropArray *pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const;
		
			//-------------------------------------------------------------------------------------
			// Derived Plan Properties
			//-------------------------------------------------------------------------------------

			// derive sort order
			virtual
			COrderSpec *PosDerive(CMemoryPool *mp, CExpressionHandle &exprhdl) const;

			//-------------------------------------------------------------------------------------
			// Enforced Properties
			//-------------------------------------------------------------------------------------

			// return order property enforcing type for this operator
			virtual
			CEnfdProp::EPropEnforcingType EpetOrder
				(
				CExpressionHandle &exprhdl,
				const CEnfdOrder *peo
				) 
				const;
			
			// compute required output columns of the n-th child
			virtual
			CColRefSet *PcrsRequired
				(
				CMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CColRefSet *pcrsRequired,
				ULONG child_index,
				CDrvdPropArray *pdrgpdpCtxt,
				ULONG ulOptReq
				);

			// compute required ctes of the n-th child
			virtual
			CCTEReq *PcteRequired
				(
				CMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CCTEReq *pcter,
				ULONG child_index,
				CDrvdPropArray *pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const;

			// compute required distribution of the n-th child
			virtual
			CDistributionSpec *PdsRequired
				(
				CMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CDistributionSpec *pdsRequired,
				ULONG child_index,
				CDrvdPropArray *pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const;

			// compute required rewindability of the n-th child
			virtual
			CRewindabilitySpec *PrsRequired
				(
				CMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CRewindabilitySpec *prsRequired,
				ULONG child_index,
				CDrvdPropArray *pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const;

			// check if required columns are included in output columns
			virtual
			BOOL FProvidesReqdCols(CExpressionHandle &exprhdl, CColRefSet *pcrsRequired, ULONG ulOptReq) const;

			
			// distribution matching type
			virtual
			CEnfdDistribution::EDistributionMatching Edm
				(
				CReqdPropPlan *, // prppInput
				ULONG,  // child_index
				CDrvdPropArray *, //pdrgpdpCtxt
				ULONG // ulOptReq
				)
			{
				if (CDistributionSpec::EdtSingleton == m_pds->Edt())
				{
					// if target table is master only, request simple satisfiability, as it will not introduce duplicates
					return CEnfdDistribution::EdmSatisfy;
				}
				
				// avoid duplicates by requesting exact matching of non-singleton distributions
				return CEnfdDistribution::EdmExact;
			}
			
			// compute required partition propagation of the n-th child
			virtual
			CPartitionPropagationSpec *PppsRequired
				(
				CMemoryPool *mp,
				CExpressionHandle &exprhdl,
				CPartitionPropagationSpec *pppsRequired,
				ULONG child_index,
				CDrvdPropArray *pdrgpdpCtxt,
				ULONG ulOptReq
				);
			
			//-------------------------------------------------------------------------------------
			// Derived Plan Properties
			//-------------------------------------------------------------------------------------

			// derive distribution
			virtual
			CDistributionSpec *PdsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl) const;

			// derive rewindability
			virtual
			CRewindabilitySpec *PrsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl) const;

			// derive partition index map
			virtual
			CPartIndexMap *PpimDerive
				(
				CMemoryPool *, // mp
				CExpressionHandle &exprhdl,
				CDrvdPropCtxt * //pdpctxt
				)
				const
			{
				return PpimPassThruOuter(exprhdl);
			}
			
			// derive partition filter map
			virtual
			CPartFilterMap *PpfmDerive
				(
				CMemoryPool *, // mp
				CExpressionHandle &exprhdl
				)
				const
			{
				return PpfmPassThruOuter(exprhdl);
			}

			//-------------------------------------------------------------------------------------
			// Enforced Properties
			//-------------------------------------------------------------------------------------


			// return rewindability property enforcing type for this operator
			virtual
			CEnfdProp::EPropEnforcingType EpetRewindability
				(
				CExpressionHandle &, // exprhdl
				const CEnfdRewindability * // per
				)
				const;

			// return true if operator passes through stats obtained from children,
			// this is used when computing stats during costing
			virtual
			BOOL FPassThruStats() const
			{
				return false;
			}

			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------

			// conversion function
			static
			CPhysicalDML *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(COperator::EopPhysicalDML == pop->Eopid());

				return dynamic_cast<CPhysicalDML*>(pop);
			}

			// debug print
			virtual 
			IOstream &OsPrint(IOstream &os) const;

	}; // class CPhysicalDML

}


#endif // !GPOS_CPhysicalDML_H

// EOF

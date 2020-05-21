//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalSplit.h
//
//	@doc:
//		Physical split operator
//---------------------------------------------------------------------------
#ifndef GPOS_CPhysicalSplit_H
#define GPOS_CPhysicalSplit_H

#include "gpos/base.h"
#include "gpopt/operators/CPhysical.h"

namespace gpopt
{
	// fwd declaration
	class CDistributionSpec;

	//---------------------------------------------------------------------------
	//	@class:
	//		CPhysicalSplit
	//
	//	@doc:
	//		Physical split operator
	//
	//---------------------------------------------------------------------------
	class CPhysicalSplit : public CPhysical
	{
		private:

			// deletion columns
			CColRefArray *m_pdrgpcrDelete;

			// insertion columns
			CColRefArray *m_pdrgpcrInsert;

			// ctid column
			CColRef *m_pcrCtid;

			// segmentid column
			CColRef *m_pcrSegmentId;

			// action column
			CColRef *m_pcrAction;
			
			// tuple oid column
			CColRef *m_pcrTupleOid;
			
			// required columns by local members
			CColRefSet *m_pcrsRequiredLocal;

			// private copy ctor
			CPhysicalSplit(const CPhysicalSplit &);

		public:

			// ctor
			CPhysicalSplit
				(
				CMemoryPool *mp,
				CColRefArray *pdrgpcrDelete,
				CColRefArray *pdrgpcrInsert,
				CColRef *pcrCtid,
				CColRef *pcrSegmentId,
				CColRef *pcrAction,
				CColRef *pcrTupleOid
				);

			// dtor
			virtual
			~CPhysicalSplit();

			// ident accessors
			virtual
			EOperatorId Eopid() const
			{
				return EopPhysicalSplit;
			}

			// return a string for operator name
			virtual
			const CHAR *SzId() const
			{
				return "CPhysicalSplit";
			}

			// action column
			CColRef *PcrAction() const
			{
				return m_pcrAction;
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

			// deletion columns
			CColRefArray *PdrgpcrDelete() const
			{
				return m_pdrgpcrDelete;
			}

			// insertion columns
			CColRefArray *PdrgpcrInsert() const
			{
				return m_pdrgpcrInsert;
			}

			// tuple oid column
			CColRef *PcrTupleOid() const
			{
				return m_pcrTupleOid;
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
			CPhysicalSplit *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(COperator::EopPhysicalSplit == pop->Eopid());

				return dynamic_cast<CPhysicalSplit*>(pop);
			}

			// debug print
			virtual
			IOstream &OsPrint(IOstream &os) const;

	}; // class CPhysicalSplit
}

#endif // !GPOS_CPhysicalSplit_H

// EOF

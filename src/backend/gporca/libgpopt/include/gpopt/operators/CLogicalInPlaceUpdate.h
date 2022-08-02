//---------------------------------------------------------------------------
//
//	@filename:
//		CLogicalInPlaceUpdate.h
//
//	@doc:
//		Logical InPlaceUpdate operator
//---------------------------------------------------------------------------
#ifndef GPOPT_MASTER_CLOGICALINPLACEUPDATE_H
#define GPOPT_MASTER_CLOGICALINPLACEUPDATE_H

#include "gpos/base.h"

#include "gpopt/operators/CLogical.h"

namespace gpopt
{
// fwd declarations
class CTableDescriptor;

//---------------------------------------------------------------------------
//	@class:
//		CLogicalInPlaceUpdate
//
//	@doc:
//		Logical InPlaceUpdate operator
//
//---------------------------------------------------------------------------
class CLogicalInPlaceUpdate : public CLogical
{
private:
	// table descriptor
	CTableDescriptor *m_ptabdesc;

	// columns to insert
	CColRefArray *m_pdrgpcrInsert;

	// columns to delete
	CColRefArray *m_pdrgpcrDelete;

	// ctid column
	CColRef *m_pcrCtid;

	// segmentId column
	CColRef *m_pcrSegmentId;

public:
	CLogicalInPlaceUpdate(const CLogicalInPlaceUpdate &) = delete;

	// ctor
	explicit CLogicalInPlaceUpdate(CMemoryPool *mp);

	// ctor
	CLogicalInPlaceUpdate(CMemoryPool *mp, CTableDescriptor *ptabdesc,
						  CColRefArray *pdrgpcrInsert,
						  CColRefArray *m_pdrgpcrDelete, CColRef *pcrCtid,
						  CColRef *pcrSegmentId);

	// dtor
	~CLogicalInPlaceUpdate() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalInPlaceUpdate;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalInPlaceUpdate";
	}

	// columns to insert
	CColRefArray *
	PdrgpcrInsert() const
	{
		return m_pdrgpcrInsert;
	}

	// columns to delete
	CColRefArray *
	PdrgpcrDelete() const
	{
		return m_pdrgpcrDelete;
	}

	// ctid column
	CColRef *
	PcrCtid() const
	{
		return m_pcrCtid;
	}

	// segmentId column
	CColRef *
	PcrSegmentId() const
	{
		return m_pcrSegmentId;
	}

	// return table's descriptor
	CTableDescriptor *
	Ptabdesc() const
	{
		return m_ptabdesc;
	}

	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *pop) const override;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return false;
	}

	// return a copy of the operator with remapped columns
	COperator *PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist) override;

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive output columns
	CColRefSet *DeriveOutputColumns(CMemoryPool *mp,
									CExpressionHandle &exprhdl) override;


	// derive constraint property
	CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *,	 // mp
							 CExpressionHandle &exprhdl) const override
	{
		return CLogical::PpcDeriveConstraintPassThru(exprhdl, 0 /*ulChild*/);
	}

	// derive max card
	CMaxCard DeriveMaxCard(CMemoryPool *mp,
						   CExpressionHandle &exprhdl) const override;

	// derive partition consumer info
	CPartInfo *
	DerivePartitionInfo(CMemoryPool *,	// mp,
						CExpressionHandle &exprhdl) const override
	{
		return PpartinfoPassThruOuter(exprhdl);
	}

	// compute required stats columns of the n-th child
	CColRefSet *
	PcrsStat(CMemoryPool *,		   // mp
			 CExpressionHandle &,  // exprhdl
			 CColRefSet *pcrsInput,
			 ULONG	// child_index
	) const override
	{
		return PcrsStatsPassThru(pcrsInput);
	}

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	// derive key collections
	CKeyCollection *DeriveKeyCollection(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const override;

	// derive statistics
	IStatistics *PstatsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  IStatisticsArray *stats_ctxt) const override;

	// stat promise
	EStatPromise
	Esp(CExpressionHandle &) const override
	{
		return CLogical::EspHigh;
	}

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalInPlaceUpdate *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalInPlaceUpdate == pop->Eopid());

		return dynamic_cast<CLogicalInPlaceUpdate *>(pop);
	}

	// debug print
	IOstream &OsPrint(IOstream &) const override;

};	// class CLogicalInPlaceUpdate
}  // namespace gpopt


#endif	//GPOPT_MASTER_CLOGICALINPLACEUPDATE_H
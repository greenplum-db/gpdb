//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalDelete.h
//
//	@doc:
//		Logical Delete operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalDelete_H
#define GPOPT_CLogicalDelete_H

#include "gpos/base.h"

#include "gpopt/operators/CLogical.h"

namespace gpopt
{
// fwd declarations
class CTableDescriptor;

//---------------------------------------------------------------------------
//	@class:
//		CLogicalDelete
//
//	@doc:
//		Logical Delete operator
//
//---------------------------------------------------------------------------
class CLogicalDelete : public CLogical
{
private:
	// table descriptor
	CTableDescriptor *m_ptabdesc;

	// columns to delete
	CColRefArray *m_pdrgpcr;

	// ctid column
	CColRef *m_pcrCtid;

	// segmentId column
	CColRef *m_pcrSegmentId;

public:
	CLogicalDelete(const CLogicalDelete &) = delete;

	// ctor
	explicit CLogicalDelete(CMemoryPool *mp);

	// ctor
	CLogicalDelete(CMemoryPool *mp, CTableDescriptor *ptabdesc,
				   CColRefArray *colref_array, CColRef *pcrCtid,
				   CColRef *pcrSegmentId);

	// dtor
	~CLogicalDelete() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalDelete;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalDelete";
	}

	// columns to delete
	CColRefArray *
	Pdrgpcr() const
	{
		return m_pdrgpcr;
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
	static CLogicalDelete *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalDelete == pop->Eopid());

		return dynamic_cast<CLogicalDelete *>(pop);
	}

	// debug print
	IOstream &OsPrint(IOstream &) const override;

};	// class CLogicalDelete
}  // namespace gpopt

#endif	// !GPOPT_CLogicalDelete_H

// EOF

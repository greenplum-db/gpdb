//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalSplit.h
//
//	@doc:
//		Logical split operator used for DML updates
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalSplit_H
#define GPOPT_CLogicalSplit_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogical.h"

namespace gpopt
{
// fwd declarations
class CTableDescriptor;

//---------------------------------------------------------------------------
//	@class:
//		CLogicalSplit
//
//	@doc:
//		Logical split operator
//
//---------------------------------------------------------------------------
class CLogicalSplit : public CLogical
{
private:
	// deletion columns
	CColRefArray *m_pdrgpcrDelete;

	// insertion columns
	CColRefArray *m_pdrgpcrInsert;

	// ctid column
	CColRef *m_pcrCtid;

	// segmentId column
	CColRef *m_pcrSegmentId;

	// action column
	CColRef *m_pcrAction;

public:
	CLogicalSplit(const CLogicalSplit &) = delete;

	// ctor
	explicit CLogicalSplit(CMemoryPool *mp);

	// ctor
	CLogicalSplit(CMemoryPool *mp, CColRefArray *pdrgpcrDelete,
				  CColRefArray *pdrgpcrInsert, CColRef *pcrCtid,
				  CColRef *pcrSegmentId, CColRef *pcrAction);

	// dtor
	~CLogicalSplit() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalSplit;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalSplit";
	}

	// deletion columns
	CColRefArray *
	PdrgpcrDelete() const
	{
		return m_pdrgpcrDelete;
	}

	// insertion columns
	CColRefArray *
	PdrgpcrInsert() const
	{
		return m_pdrgpcrInsert;
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

	// action column
	CColRef *
	PcrAction() const
	{
		return m_pcrAction;
	}

	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *pop) const override;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return true;
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
	PcrsStat(CMemoryPool *mp, CExpressionHandle &exprhdl, CColRefSet *pcrsInput,
			 ULONG child_index) const override
	{
		return PcrsReqdChildStats(mp, exprhdl, pcrsInput,
								  exprhdl.DeriveUsedColumns(1), child_index);
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
	static CLogicalSplit *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalSplit == pop->Eopid());

		return dynamic_cast<CLogicalSplit *>(pop);
	}

	// debug print
	IOstream &OsPrint(IOstream &) const override;

};	// class CLogicalSplit
}  // namespace gpopt

#endif	// !GPOPT_CLogicalSplit_H

// EOF

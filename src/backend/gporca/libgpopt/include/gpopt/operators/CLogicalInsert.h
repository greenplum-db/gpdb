//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalInsert.h
//
//	@doc:
//		Logical Insert operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalInsert_H
#define GPOPT_CLogicalInsert_H

#include "gpos/base.h"

#include "gpopt/operators/CLogical.h"

namespace gpopt
{
// fwd declarations
class CTableDescriptor;

//---------------------------------------------------------------------------
//	@class:
//		CLogicalInsert
//
//	@doc:
//		Logical Insert operator
//
//---------------------------------------------------------------------------
class CLogicalInsert : public CLogical
{
private:
	// table descriptor
	CTableDescriptor *m_ptabdesc;

	// source columns
	CColRefArray *m_pdrgpcrSource;

public:
	CLogicalInsert(const CLogicalInsert &) = delete;

	// ctor
	explicit CLogicalInsert(CMemoryPool *mp);

	// ctor
	CLogicalInsert(CMemoryPool *mp, CTableDescriptor *ptabdesc,
				   CColRefArray *colref_array);

	// dtor
	~CLogicalInsert() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalInsert;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalInsert";
	}

	// source columns
	CColRefArray *
	PdrgpcrSource() const
	{
		return m_pdrgpcrSource;
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
	static CLogicalInsert *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalInsert == pop->Eopid());

		return dynamic_cast<CLogicalInsert *>(pop);
	}

	// debug print
	IOstream &OsPrint(IOstream &) const override;

};	// class CLogicalInsert
}  // namespace gpopt

#endif	// !GPOPT_CLogicalInsert_H

// EOF

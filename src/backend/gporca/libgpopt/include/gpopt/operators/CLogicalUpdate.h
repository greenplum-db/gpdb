//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalUpdate.h
//
//	@doc:
//		Logical Update operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalUpdate_H
#define GPOPT_CLogicalUpdate_H

#include "gpos/base.h"

#include "gpopt/operators/CLogical.h"

namespace gpopt
{
// fwd declarations
class CTableDescriptor;

//---------------------------------------------------------------------------
//	@class:
//		CLogicalUpdate
//
//	@doc:
//		Logical Update operator
//
//---------------------------------------------------------------------------
class CLogicalUpdate : public CLogical
{
private:
	// table descriptor
	CTableDescriptor *m_ptabdesc;

	// columns to delete
	CColRefArray *m_pdrgpcrDelete;

	// columns to insert
	CColRefArray *m_pdrgpcrInsert;

	// ctid column
	CColRef *m_pcrCtid;

	// segmentId column
	CColRef *m_pcrSegmentId;

	// Split Update
	BOOL m_fSplit;

public:
	CLogicalUpdate(const CLogicalUpdate &) = delete;

	// ctor
	explicit CLogicalUpdate(CMemoryPool *mp);

	// ctor
	CLogicalUpdate(CMemoryPool *mp, CTableDescriptor *ptabdesc,
				   CColRefArray *pdrgpcrDelete, CColRefArray *pdrgpcrInsert,
				   CColRef *pcrCtid, CColRef *pcrSegmentId, BOOL fSplit);

	// dtor
	~CLogicalUpdate() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalUpdate;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalUpdate";
	}

	// columns to delete
	CColRefArray *
	PdrgpcrDelete() const
	{
		return m_pdrgpcrDelete;
	}

	// columns to insert
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

	// return table's descriptor
	CTableDescriptor *
	Ptabdesc() const
	{
		return m_ptabdesc;
	}

	// Is update using split
	BOOL
	FSplit() const
	{
		return m_fSplit;
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
	static CLogicalUpdate *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalUpdate == pop->Eopid());

		return dynamic_cast<CLogicalUpdate *>(pop);
	}

	// debug print
	IOstream &OsPrint(IOstream &) const override;

};	// class CLogicalUpdate
}  // namespace gpopt

#endif	// !GPOPT_CLogicalUpdate_H

// EOF

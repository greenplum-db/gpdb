//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CLogicalPartitionSelector.h
//
//	@doc:
//		Logical partition selector operator. This is used for DML
//		on partitioned tables
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalPartitionSelector_H
#define GPOPT_CLogicalPartitionSelector_H

#include "gpos/base.h"
#include "gpopt/operators/CLogical.h"


namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalPartitionSelector
//
//	@doc:
//		Logical partition selector operator
//
//---------------------------------------------------------------------------
class CLogicalPartitionSelector : public CLogical
{
private:
	// mdid of partitioned table
	IMDId *m_mdid;

	// filter expressions corresponding to various levels
	CExpressionArray *m_pdrgpexprFilters;

	// oid column - holds the OIDs for leaf parts
	CColRef *m_pcrOid;

public:
	CLogicalPartitionSelector(const CLogicalPartitionSelector &) = delete;

	// ctors
	explicit CLogicalPartitionSelector(CMemoryPool *mp);

	CLogicalPartitionSelector(CMemoryPool *mp, IMDId *mdid,
							  CExpressionArray *pdrgpexprFilters,
							  CColRef *pcrOid);

	// dtor
	~CLogicalPartitionSelector() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalPartitionSelector;
	}

	// operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalPartitionSelector";
	}

	// partitioned table mdid
	IMDId *
	MDId() const
	{
		return m_mdid;
	}

	// oid column
	CColRef *
	PcrOid() const
	{
		return m_pcrOid;
	}

	// number of partitioning levels
	ULONG
	UlPartLevels() const
	{
		return m_pdrgpexprFilters->Size();
	}

	// filter expression for a given level
	CExpression *
	PexprPartFilter(ULONG ulLevel) const
	{
		return (*m_pdrgpexprFilters)[ulLevel];
	}

	// match function
	BOOL Matches(COperator *pop) const override;

	// hash function
	ULONG HashValue() const override;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		// operator has one child
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
	DerivePropertyConstraint(CMemoryPool *,	 //mp,
							 CExpressionHandle &exprhdl) const override
	{
		return PpcDeriveConstraintPassThru(exprhdl, 0 /*ulChild*/);
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
	CKeyCollection *
	DeriveKeyCollection(CMemoryPool *,	// mp
						CExpressionHandle &exprhdl) const override
	{
		return PkcDeriveKeysPassThru(exprhdl, 0 /* ulChild */);
	}


	// derive statistics
	IStatistics *
	PstatsDerive(CMemoryPool *,	 //mp,
				 CExpressionHandle &exprhdl,
				 IStatisticsArray *	 //stats_ctxt
	) const override
	{
		return PstatsPassThruOuter(exprhdl);
	}

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
	static CLogicalPartitionSelector *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopLogicalPartitionSelector == pop->Eopid());

		return dynamic_cast<CLogicalPartitionSelector *>(pop);
	}

	// debug print
	IOstream &OsPrint(IOstream &) const override;

};	// class CLogicalPartitionSelector

}  // namespace gpopt

#endif	// !GPOPT_CLogicalPartitionSelector_H

// EOF

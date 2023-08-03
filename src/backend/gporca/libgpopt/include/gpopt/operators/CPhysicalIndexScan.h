//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp
//
//	@filename:
//		CPhysicalIndexScan.h
//
//	@doc:
//		Base class for physical index scan operators
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalIndexScan_H
#define GPOPT_CPhysicalIndexScan_H

#include "gpos/base.h"

#include "gpopt/metadata/CIndexDescriptor.h"
#include "gpopt/operators/CPhysicalScan.h"

namespace gpopt
{
// fwd declarations
class CTableDescriptor;
class CIndexDescriptor;
class CName;
class CDistributionSpecHashed;

//---------------------------------------------------------------------------
//	@class:
//		CPhysicalIndexScan
//
//	@doc:
//		Base class for physical index scan operators
//
//---------------------------------------------------------------------------
class CPhysicalIndexScan : public CPhysicalScan
{
private:
	// index descriptor
	CIndexDescriptor *m_pindexdesc;

	// origin operator id -- gpos::ulong_max if operator was not generated via a transformation
	ULONG m_ulOriginOpId;

	// order
	COrderSpec *m_pos;

	// Number of predicate not applicable on the index
	ULONG m_ulUnindexedPredColCount;

	// index scan direction
	EIndexScanDirection m_scan_direction;

public:
	CPhysicalIndexScan(const CPhysicalIndexScan &) = delete;

	// ctors
	CPhysicalIndexScan(CMemoryPool *mp, CIndexDescriptor *pindexdesc,
					   CTableDescriptor *ptabdesc, ULONG ulOriginOpId,
					   const CName *pnameAlias, CColRefArray *colref_array,
					   COrderSpec *pos, ULONG ulUnindexedPredColCount,
					   EIndexScanDirection scan_direction);

	// dtor
	~CPhysicalIndexScan() override;


	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalIndexScan;
	}

	// operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalIndexScan";
	}

	// table alias name
	const CName &
	NameAlias() const
	{
		return *m_pnameAlias;
	}

	// origin operator id -- gpos::ulong_max if operator was not generated via a transformation
	ULONG
	UlOriginOpId() const
	{
		return m_ulOriginOpId;
	}

	// index scan direction is only used for B-tree indices.
	EIndexScanDirection
	IndexScanDirection() const
	{
		return m_scan_direction;
	}

	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *pop) const override;

	// index descriptor
	CIndexDescriptor *
	Pindexdesc() const
	{
		return m_pindexdesc;
	}

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return true;
	}

	// number of predicate not applicable on the index
	ULONG
	ResidualPredicateSize() const
	{
		return m_ulUnindexedPredColCount;
	}

	//-------------------------------------------------------------------------------------
	// Derived Plan Properties
	//-------------------------------------------------------------------------------------

	// derive sort order
	COrderSpec *
	PosDerive(CMemoryPool *,	   //mp
			  CExpressionHandle &  //exprhdl
	) const override
	{
		m_pos->AddRef();
		return m_pos;
	}

	CRewindabilitySpec *
	PrsDerive(CMemoryPool *mp,
			  CExpressionHandle &  // exprhdl
	) const override
	{
		// rewindability of output is always true
		return GPOS_NEW(mp)
			CRewindabilitySpec(CRewindabilitySpec::ErtMarkRestore,
							   CRewindabilitySpec::EmhtNoMotion);
	}

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	//-------------------------------------------------------------------------------------

	// return order property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetOrder(
		CExpressionHandle &exprhdl, const CEnfdOrder *peo) const override;

	// conversion function
	static CPhysicalIndexScan *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopPhysicalIndexScan == pop->Eopid());

		return dynamic_cast<CPhysicalIndexScan *>(pop);
	}

	// statistics derivation during costing
	IStatistics *
	PstatsDerive(CMemoryPool *,		   // mp
				 CExpressionHandle &,  // exprhdl
				 CReqdPropPlan *,	   // prpplan
				 IStatisticsArray *	   //stats_ctxt
	) const override
	{
		GPOS_ASSERT(
			!"stats derivation during costing for index scan is invalid");

		return nullptr;
	}

	// debug print
	IOstream &OsPrint(IOstream &) const override;

};	// class CPhysicalIndexScan

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalIndexScan_H

// EOF

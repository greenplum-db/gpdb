//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp
//
//	@filename:
//		CPhysicalDynamicIndexScan.h
//
//	@doc:
//		Physical dynamic index scan operators on partitioned tables
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalDynamicIndexScan_H
#define GPOPT_CPhysicalDynamicIndexScan_H

#include "gpos/base.h"

#include "gpopt/metadata/CIndexDescriptor.h"
#include "gpopt/operators/CPhysicalDynamicScan.h"

namespace gpopt
{
// fwd declarations
class CTableDescriptor;
class CIndexDescriptor;
class CName;
class CPartConstraint;

//---------------------------------------------------------------------------
//	@class:
//		CPhysicalDynamicIndexScan
//
//	@doc:
//		Physical dynamic index scan operators for partitioned tables
//
//---------------------------------------------------------------------------
class CPhysicalDynamicIndexScan : public CPhysicalDynamicScan
{
private:
	// index descriptor
	CIndexDescriptor *m_pindexdesc;

	// order
	COrderSpec *m_pos;

	// Number of predicate not applicable on the index
	ULONG m_ulUnindexedPredColCount;

public:
	CPhysicalDynamicIndexScan(const CPhysicalDynamicIndexScan &) = delete;

	// ctors
	CPhysicalDynamicIndexScan(CMemoryPool *mp, CIndexDescriptor *pindexdesc,
							  CTableDescriptor *ptabdesc, ULONG ulOriginOpId,
							  const CName *pnameAlias,
							  CColRefArray *pdrgpcrOutput, ULONG scan_id,
							  CColRef2dArray *pdrgpdrgpcrPart, COrderSpec *pos,
							  IMdIdArray *partition_mdids,
							  ColRefToUlongMapArray *root_col_mapping_per_part,
							  ULONG ulUnindexedPredColCount);

	// dtor
	~CPhysicalDynamicIndexScan() override;


	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalDynamicIndexScan;
	}

	// operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalDynamicIndexScan";
	}

	// index descriptor
	CIndexDescriptor *
	Pindexdesc() const
	{
		return m_pindexdesc;
	}

	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *pop) const override;

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

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	//-------------------------------------------------------------------------------------

	// return order property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetOrder(
		CExpressionHandle &exprhdl, const CEnfdOrder *peo) const override;

	// conversion function
	static CPhysicalDynamicIndexScan *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopPhysicalDynamicIndexScan == pop->Eopid());

		return dynamic_cast<CPhysicalDynamicIndexScan *>(pop);
	}

	// debug print
	IOstream &OsPrint(IOstream &) const override;

	// statistics derivation during costing
	IStatistics *PstatsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  CReqdPropPlan *prpplan,
							  IStatisticsArray *stats_ctxt) const override;

};	// class CPhysicalDynamicIndexScan

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalDynamicIndexScan_H

// EOF

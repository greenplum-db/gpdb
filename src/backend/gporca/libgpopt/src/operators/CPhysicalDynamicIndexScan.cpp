//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalDynamicIndexScan.cpp
//
//	@doc:
//		Implementation of dynamic index scan operator
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/error/CAutoTrace.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/metadata/CPartConstraint.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPhysicalDynamicIndexScan.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/cost/ICostModel.h"

#include "naucrates/statistics/CStatisticsUtils.h"
#include "naucrates/statistics/CFilterStatsProcessor.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicIndexScan::CPhysicalDynamicIndexScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalDynamicIndexScan::CPhysicalDynamicIndexScan(
	CMemoryPool *mp, BOOL is_partial, CIndexDescriptor *pindexdesc,
	CTableDescriptor *ptabdesc, ULONG ulOriginOpId, const CName *pnameAlias,
	CColRefArray *pdrgpcrOutput, ULONG scan_id, CColRef2dArray *pdrgpdrgpcrPart,
	ULONG ulSecondaryScanId, CPartConstraint *ppartcnstr,
	CPartConstraint *ppartcnstrRel, COrderSpec *pos)
	: CPhysicalDynamicScan(mp, is_partial, ptabdesc, ulOriginOpId, pnameAlias,
						   scan_id, pdrgpcrOutput, pdrgpdrgpcrPart,
						   ulSecondaryScanId, ppartcnstr, ppartcnstrRel),
	  m_pindexdesc(pindexdesc),
	  m_pos(pos)
{
	GPOS_ASSERT(NULL != pindexdesc);
	GPOS_ASSERT(NULL != pos);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicIndexScan::~CPhysicalDynamicIndexScan
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalDynamicIndexScan::~CPhysicalDynamicIndexScan()
{
	m_pindexdesc->Release();
	m_pos->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicIndexScan::EpetOrder
//
//	@doc:
//		Return the enforcing type for order property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalDynamicIndexScan::EpetOrder(CExpressionHandle &,  // exprhdl
									 const CEnfdOrder *peo) const
{
	GPOS_ASSERT(NULL != peo);
	GPOS_ASSERT(!peo->PosRequired()->IsEmpty());

	if (peo->FCompatible(m_pos))
	{
		// required order is already established by the index
		return CEnfdProp::EpetUnnecessary;
	}

	return CEnfdProp::EpetRequired;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicIndexScan::HashValue
//
//	@doc:
//		Combine pointers for table descriptor, index descriptor and Eop
//
//---------------------------------------------------------------------------
ULONG
CPhysicalDynamicIndexScan::HashValue() const
{
	ULONG scan_id = ScanId();
	return gpos::CombineHashes(
		COperator::HashValue(),
		gpos::CombineHashes(gpos::HashValue(&scan_id),
							m_pindexdesc->MDId()->HashValue()));
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicIndexScan::Matches
//
//	@doc:
//		match operator
//
//---------------------------------------------------------------------------
BOOL
CPhysicalDynamicIndexScan::Matches(COperator *pop) const
{
	return CUtils::FMatchDynamicIndex(this, pop);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicIndexScan::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CPhysicalDynamicIndexScan::OsPrint(IOstream &os) const
{
	if (m_fPattern)
	{
		return COperator::OsPrint(os);
	}

	os << SzId() << " ";
	// index name
	os << "  Index: (";
	m_pindexdesc->Name().OsPrint(os);
	// table name
	os << ")";
	os << ", Table: (";
	Ptabdesc()->Name().OsPrint(os);
	os << ")";
	os << ", Columns: [";
	CUtils::OsPrintDrgPcr(os, PdrgpcrOutput());
	os << "] Scan Id: " << ScanId() << "." << UlSecondaryScanId();

	if (!Ppartcnstr()->IsConstraintUnbounded())
	{
		os << ", ";
		Ppartcnstr()->OsPrint(os);
	}

	return os;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicIndexScan::PstatsDerive
//
//	@doc:
//		Statistics derivation during costing
//
//---------------------------------------------------------------------------
IStatistics *
CPhysicalDynamicIndexScan::PstatsDerive(CMemoryPool *mp,
										CExpressionHandle &exprhdl,
										CReqdPropPlan *prpplan,
										IStatisticsArray *stats_ctxt) const
{
	GPOS_ASSERT(NULL != prpplan);

	IStatistics *pstatsBaseTable = CStatisticsUtils::DeriveStatsForDynamicScan(
		mp, exprhdl, ScanId(), prpplan->Pepp()->PpfmDerived());

	// create a conjunction of index condition and additional filters
	CExpression *pexprScalar = exprhdl.PexprScalarRepChild(0 /*ulChidIndex*/);
	CExpression *local_expr = NULL;
	CExpression *expr_with_outer_refs = NULL;

	// get outer references from expression handle
	CColRefSet *outer_refs = exprhdl.DeriveOuterReferences();

	CPredicateUtils::SeparateOuterRefs(mp, pexprScalar, outer_refs, &local_expr,
									   &expr_with_outer_refs);

	IStatistics *stats = CFilterStatsProcessor::MakeStatsFilterForScalarExpr(
		mp, exprhdl, pstatsBaseTable, local_expr, expr_with_outer_refs,
		stats_ctxt);

	pstatsBaseTable->Release();
	local_expr->Release();
	expr_with_outer_refs->Release();

	return stats;
}

// EOF

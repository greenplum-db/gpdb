//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal, Inc.
//
//	@filename:
//		CPhysicalDynamicScan.cpp
//
//	@doc:
//		Base class for physical dynamic scan operators
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#include "gpopt/operators/CPhysicalDynamicScan.h"

#include "gpopt/base/CDrvdPropCtxtPlan.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/metadata/CName.h"
#include "gpopt/metadata/CPartConstraint.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CExpressionHandle.h"

using namespace gpopt;
using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicScan::CPhysicalDynamicScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalDynamicScan::CPhysicalDynamicScan(
	CMemoryPool *mp, BOOL is_partial, CTableDescriptor *ptabdesc,
	ULONG ulOriginOpId, const CName *pnameAlias, ULONG scan_id,
	CColRefArray *pdrgpcrOutput, CColRef2dArray *pdrgpdrgpcrParts,
	ULONG ulSecondaryScanId, CPartConstraint *ppartcnstr,
	CPartConstraint *ppartcnstrRel)
	: CPhysicalScan(mp, pnameAlias, ptabdesc, pdrgpcrOutput),
	  m_ulOriginOpId(ulOriginOpId),
	  m_is_partial(is_partial),
	  m_scan_id(scan_id),
	  m_pdrgpdrgpcrPart(pdrgpdrgpcrParts),
	  m_ulSecondaryScanId(ulSecondaryScanId),
	  m_part_constraint(ppartcnstr),
	  m_ppartcnstrRel(ppartcnstrRel)
{
	GPOS_ASSERT(NULL != pdrgpdrgpcrParts);
	GPOS_ASSERT(0 < pdrgpdrgpcrParts->Size());
	GPOS_ASSERT(NULL != ppartcnstr);
	GPOS_ASSERT(NULL != ppartcnstrRel);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicScan::~CPhysicalDynamicScan
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalDynamicScan::~CPhysicalDynamicScan()
{
	m_pdrgpdrgpcrPart->Release();
	m_part_constraint->Release();
	m_ppartcnstrRel->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicScan::HashValue
//
//	@doc:
//		Combine part index, pointer for table descriptor, Eop and output columns
//
//---------------------------------------------------------------------------
ULONG
CPhysicalDynamicScan::HashValue() const
{
	ULONG ulHash = gpos::CombineHashes(
		COperator::HashValue(),
		gpos::CombineHashes(gpos::HashValue(&m_scan_id),
							m_ptabdesc->MDId()->HashValue()));
	ulHash =
		gpos::CombineHashes(ulHash, CUtils::UlHashColArray(m_pdrgpcrOutput));

	return ulHash;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicScan::PpimDerive
//
//	@doc:
//		Derive partition index map
//
//---------------------------------------------------------------------------
CPartIndexMap *
CPhysicalDynamicScan::PpimDerive(CMemoryPool *mp,
								 CExpressionHandle &,  //exprhdl
								 CDrvdPropCtxt *pdpctxt) const
{
	GPOS_ASSERT(NULL != pdpctxt);
	IMDId *mdid = m_ptabdesc->MDId();
	mdid->AddRef();
	m_pdrgpdrgpcrPart->AddRef();
	m_part_constraint->AddRef();
	m_ppartcnstrRel->AddRef();
	ULONG ulExpectedPartitionSelectors =
		CDrvdPropCtxtPlan::PdpctxtplanConvert(pdpctxt)
			->UlExpectedPartitionSelectors();

	return PpimDeriveFromDynamicScan(
		mp, m_scan_id, mdid, m_pdrgpdrgpcrPart, m_ulSecondaryScanId,
		m_part_constraint, m_ppartcnstrRel, ulExpectedPartitionSelectors);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicScan::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CPhysicalDynamicScan::OsPrint(IOstream &os) const
{
	os << SzId() << " ";

	// alias of table as referenced in the query
	m_pnameAlias->OsPrint(os);

	// actual name of table in catalog and columns
	os << " (";
	m_ptabdesc->Name().OsPrint(os);
	os << "), Columns: [";
	CUtils::OsPrintDrgPcr(os, m_pdrgpcrOutput);
	os << "] Scan Id: " << m_scan_id << "." << m_ulSecondaryScanId;

	if (!m_part_constraint->IsConstraintUnbounded())
	{
		os << ", ";
		m_part_constraint->OsPrint(os);
	}

	return os;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicScan::PopConvert
//
//	@doc:
//		conversion function
//
//---------------------------------------------------------------------------
CPhysicalDynamicScan *
CPhysicalDynamicScan::PopConvert(COperator *pop)
{
	GPOS_ASSERT(NULL != pop);
	GPOS_ASSERT(CUtils::FPhysicalScan(pop) &&
				CPhysicalScan::PopConvert(pop)->FDynamicScan());

	return dynamic_cast<CPhysicalDynamicScan *>(pop);
}


// EOF

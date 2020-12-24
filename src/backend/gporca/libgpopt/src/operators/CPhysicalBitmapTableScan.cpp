//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CPhysicalBitmapTableScan.cpp
//
//	@doc:
//		Bitmap table scan physical operator
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#include "gpopt/operators/CPhysicalBitmapTableScan.h"

#include "gpopt/base/CDistributionSpec.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/metadata/CTableDescriptor.h"

using namespace gpopt;
using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalBitmapTableScan::CPhysicalBitmapTableScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalBitmapTableScan::CPhysicalBitmapTableScan(CMemoryPool *mp,
												   CTableDescriptor *ptabdesc,
												   ULONG ulOriginOpId,
												   const CName *pnameTableAlias,
												   CColRefArray *pdrgpcrOutput)
	: CPhysicalScan(mp, pnameTableAlias, ptabdesc, pdrgpcrOutput),
	  m_ulOriginOpId(ulOriginOpId)
{
	GPOS_ASSERT(NULL != mp);
	GPOS_ASSERT(NULL != ptabdesc);
	GPOS_ASSERT(NULL != pdrgpcrOutput);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalBitmapTableScan::HashValue
//
//	@doc:
//		Operator specific hash function
//
//---------------------------------------------------------------------------
ULONG
CPhysicalBitmapTableScan::HashValue() const
{
	ULONG ulHash = gpos::CombineHashes(COperator::HashValue(),
									   m_ptabdesc->MDId()->HashValue());
	ulHash =
		gpos::CombineHashes(ulHash, CUtils::UlHashColArray(m_pdrgpcrOutput));

	return ulHash;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalBitmapTableScan::HashValue
//
//	@doc:
//		Match this operator with the given one.
//
//---------------------------------------------------------------------------
BOOL
CPhysicalBitmapTableScan::Matches(COperator *pop) const
{
	return CUtils::FMatchBitmapScan(this, pop);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalBitmapTableScan::OsPrint
//
//	@doc:
//		Debug print of a CPhysicalBitmapTableScan
//
//---------------------------------------------------------------------------
IOstream &
CPhysicalBitmapTableScan::OsPrint(IOstream &os) const
{
	os << SzId() << " ";
	os << ", Table Name: (";
	m_ptabdesc->Name().OsPrint(os);
	os << ")";
	os << ", Columns: [";
	CUtils::OsPrintDrgPcr(os, m_pdrgpcrOutput);
	os << "]";

	return os;
}

// EOF

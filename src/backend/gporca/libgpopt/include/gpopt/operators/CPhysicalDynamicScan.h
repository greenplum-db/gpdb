//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal, Inc.
//
//	@filename:
//		CPhysicalDynamicScan.h
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

#ifndef GPOPT_CPhysicalDynamicScan_H
#define GPOPT_CPhysicalDynamicScan_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalScan.h"

namespace gpopt
{
// fwd declarations
class CTableDescriptor;
class CName;
class CPartConstraint;

//---------------------------------------------------------------------------
//	@class:
//		CPhysicalDynamicScan
//
//	@doc:
//		Base class for physical dynamic scan operators
//
//---------------------------------------------------------------------------
class CPhysicalDynamicScan : public CPhysicalScan
{
private:
	// origin operator id -- gpos::ulong_max if operator was not generated via a transformation
	ULONG m_ulOriginOpId;

	// true iff it is a partial scan
	BOOL m_is_partial;

	// id of the dynamic scan
	ULONG m_scan_id;

	// partition keys
	CColRef2dArray *m_pdrgpdrgpcrPart;

	// secondary scan id in case of partial scan
	ULONG m_ulSecondaryScanId;

	// dynamic index part constraint
	CPartConstraint *m_part_constraint;

	// relation part constraint
	CPartConstraint *m_ppartcnstrRel;

	// disable copy ctor
	CPhysicalDynamicScan(const CPhysicalDynamicScan &);

public:
	// ctor
	CPhysicalDynamicScan(CMemoryPool *mp, BOOL is_partial,
						 CTableDescriptor *ptabdesc, ULONG ulOriginOpId,
						 const CName *pnameAlias, ULONG scan_id,
						 CColRefArray *pdrgpcrOutput,
						 CColRef2dArray *pdrgpdrgpcrParts,
						 ULONG ulSecondaryScanId, CPartConstraint *ppartcnstr,
						 CPartConstraint *ppartcnstrRel);

	// dtor
	virtual ~CPhysicalDynamicScan();

	// origin operator id -- gpos::ulong_max if operator was not generated via a transformation
	ULONG
	UlOriginOpId() const
	{
		return m_ulOriginOpId;
	}

	// true iff the scan is partial
	BOOL
	IsPartial() const
	{
		return m_is_partial;
	}

	// return scan id
	ULONG
	ScanId() const
	{
		return m_scan_id;
	}

	// partition keys
	CColRef2dArray *
	PdrgpdrgpcrPart() const
	{
		return m_pdrgpdrgpcrPart;
	}

	// secondary scan id
	ULONG
	UlSecondaryScanId() const
	{
		return m_ulSecondaryScanId;
	}

	// dynamic index part constraint
	CPartConstraint *
	Ppartcnstr() const
	{
		return m_part_constraint;
	}

	// relation part constraint
	CPartConstraint *
	PpartcnstrRel() const
	{
		return m_ppartcnstrRel;
	}

	// sensitivity to order of inputs
	virtual BOOL
	FInputOrderSensitive() const
	{
		return true;
	}

	// operator specific hash function
	virtual ULONG HashValue() const;

	// derive partition index map
	virtual CPartIndexMap *PpimDerive(CMemoryPool *mp,
									  CExpressionHandle &exprhdl,
									  CDrvdPropCtxt *pdpctxt) const;

	// return true if operator is dynamic scan
	virtual BOOL
	FDynamicScan() const
	{
		return true;
	}

	// debug print
	virtual IOstream &OsPrint(IOstream &) const;

	// conversion function
	static CPhysicalDynamicScan *PopConvert(COperator *pop);
};
}  // namespace gpopt

#endif	// !GPOPT_CPhysicalDynamicScan_H

// EOF

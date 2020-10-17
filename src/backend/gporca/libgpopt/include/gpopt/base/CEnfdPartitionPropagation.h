//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CEnfdPartitionPropagation.h
//
//	@doc:
//		Enforceable partition propagation property
//---------------------------------------------------------------------------
#ifndef GPOPT_CEnfdPartitionPropagation_H
#define GPOPT_CEnfdPartitionPropagation_H

#include "gpos/base.h"

#include "gpopt/base/CPartIndexMap.h"
#include "gpopt/base/CEnfdProp.h"
#include "gpopt/base/CPartitionPropagationSpec.h"


namespace gpopt
{
using namespace gpos;


//---------------------------------------------------------------------------
//	@class:
//		CEnfdPartitionPropagation
//
//	@doc:
//		Enforceable distribution property;
//
//---------------------------------------------------------------------------
class CEnfdPartitionPropagation : public CEnfdProp
{
public:
	// type of distribution matching function
	enum EPartitionPropagationMatching
	{
		EppmSatisfy = 0,
		EppmSentinel
	};

private:
	// partition propagation spec
	CPartitionPropagationSpec *m_ppps;

	// distribution matching type
	EPartitionPropagationMatching m_eppm;

	// derived part filter
	CPartFilterMap *m_ppfmDerived;

	// private copy ctor
	CEnfdPartitionPropagation(const CEnfdPartitionPropagation &);

public:
	// ctor
	CEnfdPartitionPropagation(CPartitionPropagationSpec *ppps,
							  EPartitionPropagationMatching eppm,
							  CPartFilterMap *ppfm);

	// dtor
	virtual ~CEnfdPartitionPropagation();

	// partition spec accessor
	virtual CPropSpec *
	Pps() const
	{
		return m_ppps;
	}

	// hash function
	virtual ULONG HashValue() const;

	// required propagation accessor
	CPartitionPropagationSpec *
	PppsRequired() const
	{
		return m_ppps;
	}

	// derived part filter
	CPartFilterMap *
	PpfmDerived() const
	{
		return m_ppfmDerived;
	}

	// is required partition propagation resolved by the given part index map
	BOOL FResolved(CMemoryPool *mp, CPartIndexMap *ppim) const;

	// are the dynamic scans required by the partition propagation in the scope defined by the given part index map
	BOOL FInScope(CMemoryPool *mp, CPartIndexMap *ppim) const;

	// get distribution enforcing type for the given operator
	EPropEnforcingType Epet(CExpressionHandle &exprhdl, CPhysical *popPhysical,
							BOOL fPropagationReqd) const;

	// return matching type
	EPartitionPropagationMatching
	Eppm() const
	{
		return m_eppm;
	}

	// matching function
	BOOL
	Matches(CEnfdPartitionPropagation *pepp)
	{
		GPOS_ASSERT(NULL != pepp);

		return m_eppm == pepp->Eppm() &&
			   m_ppps->Matches(pepp->PppsRequired()) &&
			   PpfmDerived()->Equals(pepp->PpfmDerived());
	}

	// print function
	virtual IOstream &OsPrint(IOstream &os) const;

	// name of propagation matching type
	static const CHAR *SzPropagationMatching(
		EPartitionPropagationMatching eppm);

};	// class CEnfdPartitionPropagation

}  // namespace gpopt


#endif	// !GPOPT_CEnfdPartitionPropagation_H

// EOF

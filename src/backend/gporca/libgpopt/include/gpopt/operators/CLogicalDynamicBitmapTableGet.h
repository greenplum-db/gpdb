//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CLogicalDynamicBitmapTableGet.h
//
//	@doc:
//		Logical operator for dynamic table access via bitmap indexes.
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPOPT_CLogicalDynamicBitmapTableGet_H
#define GPOPT_CLogicalDynamicBitmapTableGet_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalDynamicGetBase.h"

namespace gpopt
{
// fwd declarations
class CColRefSet;
class CTableDescriptor;

//---------------------------------------------------------------------------
//	@class:
//		CLogicalDynamicBitmapTableGet
//
//	@doc:
//		Logical operator for dynamic table access via bitmap indexes.
//
//---------------------------------------------------------------------------
class CLogicalDynamicBitmapTableGet : public CLogicalDynamicGetBase
{
private:
	// origin operator id -- gpos::ulong_max if operator was not generated via a transformation
	ULONG m_ulOriginOpId;

public:
	CLogicalDynamicBitmapTableGet(const CLogicalDynamicBitmapTableGet &) =
		delete;

	// ctors
	CLogicalDynamicBitmapTableGet(
		CMemoryPool *mp, CTableDescriptor *ptabdesc, ULONG ulOriginOpId,
		const CName *pnameTableAlias, ULONG ulPartIndex,
		CColRefArray *pdrgpcrOutput, CColRef2dArray *pdrgpdrgpcrPart,
		ULONG ulSecondaryPartIndexId, BOOL is_partial,
		CPartConstraint *ppartcnstr, CPartConstraint *ppartcnstrRel);

	explicit CLogicalDynamicBitmapTableGet(CMemoryPool *mp);

	// dtor
	~CLogicalDynamicBitmapTableGet() override;

	// identifier
	EOperatorId
	Eopid() const override
	{
		return EopLogicalDynamicBitmapTableGet;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalDynamicBitmapTableGet";
	}

	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *pop) const override;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return true;
	}

	// return a copy of the operator with remapped columns
	COperator *PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist) override;

	// derive outer references
	CColRefSet *DeriveOuterReferences(CMemoryPool *mp,
									  CExpressionHandle &exprhdl) override;

	// derive constraint property
	CPropConstraint *DerivePropertyConstraint(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const override;

	// compute required stat columns of the n-th child
	CColRefSet *
	PcrsStat(CMemoryPool *mp,
			 CExpressionHandle &,  // exprhdl
			 CColRefSet *,		   //pcrsInput
			 ULONG				   // child_index
	) const override
	{
		return GPOS_NEW(mp) CColRefSet(mp);
	}

	// candidate set of xforms
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	// derive statistics
	IStatistics *PstatsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  IStatisticsArray *stats_ctxt) const override;

	// stat promise
	EStatPromise
	Esp(CExpressionHandle &) const override
	{
		return CLogical::EspHigh;
	}

	// origin operator id -- gpos::ulong_max if operator was not generated via a transformation
	ULONG
	UlOriginOpId() const
	{
		return m_ulOriginOpId;
	}

	// debug print
	IOstream &OsPrint(IOstream &) const override;

	// conversion
	static CLogicalDynamicBitmapTableGet *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopLogicalDynamicBitmapTableGet == pop->Eopid());

		return dynamic_cast<CLogicalDynamicBitmapTableGet *>(pop);
	}

};	// class CLogicalDynamicBitmapTableGet
}  // namespace gpopt

#endif	// !GPOPT_CLogicalDynamicBitmapTableGet_H

// EOF

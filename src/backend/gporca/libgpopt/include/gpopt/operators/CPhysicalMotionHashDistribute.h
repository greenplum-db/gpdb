//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPhysicalMotionHashDistribute.h
//
//	@doc:
//		Physical Hash distribute motion operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalMotionHashDistribute_H
#define GPOPT_CPhysicalMotionHashDistribute_H

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/base/COrderSpec.h"
#include "gpopt/operators/CPhysicalMotion.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalMotionHashDistribute
//
//	@doc:
//		Hash distribute motion operator
//
//---------------------------------------------------------------------------
class CPhysicalMotionHashDistribute : public CPhysicalMotion
{
private:
	// hash distribution spec
	CDistributionSpecHashed *m_pdsHashed;

	// required columns in distribution spec
	CColRefSet *m_pcrsRequiredLocal;

public:
	CPhysicalMotionHashDistribute(const CPhysicalMotionHashDistribute &) =
		delete;

	// ctor
	CPhysicalMotionHashDistribute(CMemoryPool *mp,
								  CDistributionSpecHashed *pdsHashed);

	// dtor
	~CPhysicalMotionHashDistribute() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalMotionHashDistribute;
	}

	const CHAR *
	SzId() const override
	{
		return "CPhysicalMotionHashDistribute";
	}

	// output distribution accessor
	CDistributionSpec *
	Pds() const override
	{
		return m_pdsHashed;
	}

	// is motion eliminating duplicates
	BOOL
	IsDuplicateSensitive() const
	{
		return m_pdsHashed->IsDuplicateSensitive();
	}

	// match function
	BOOL Matches(COperator *) const override;

	//-------------------------------------------------------------------------------------
	// Required Plan Properties
	//-------------------------------------------------------------------------------------

	// compute required output columns of the n-th child
	CColRefSet *PcrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							 CColRefSet *pcrsInput, ULONG child_index,
							 CDrvdPropArray *pdrgpdpCtxt,
							 ULONG ulOptReq) override;

	// compute required sort order of the n-th child
	COrderSpec *PosRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							COrderSpec *posInput, ULONG child_index,
							CDrvdPropArray *pdrgpdpCtxt,
							ULONG ulOptReq) const override;

	// check if required columns are included in output columns
	BOOL FProvidesReqdCols(CExpressionHandle &exprhdl, CColRefSet *pcrsRequired,
						   ULONG ulOptReq) const override;

	//-------------------------------------------------------------------------------------
	// Derived Plan Properties
	//-------------------------------------------------------------------------------------

	// derive sort order
	COrderSpec *PosDerive(CMemoryPool *mp,
						  CExpressionHandle &exprhdl) const override;

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	//-------------------------------------------------------------------------------------

	// return order property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetOrder(
		CExpressionHandle &exprhdl, const CEnfdOrder *peo) const override;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// print
	IOstream &OsPrint(IOstream &) const override;

	// conversion function
	static CPhysicalMotionHashDistribute *PopConvert(COperator *pop);

	CDistributionSpec *PdsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
								   CDistributionSpec *pdsRequired,
								   ULONG child_index,
								   CDrvdPropArray *pdrgpdpCtxt,
								   ULONG ulOptReq) const override;

};	// class CPhysicalMotionHashDistribute

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalMotionHashDistribute_H

// EOF

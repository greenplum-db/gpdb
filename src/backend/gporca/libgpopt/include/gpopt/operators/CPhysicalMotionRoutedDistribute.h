//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalMotionRoutedDistribute.h
//
//	@doc:
//		Physical routed distribute motion operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalMotionRoutedDistribute_H
#define GPOPT_CPhysicalMotionRoutedDistribute_H

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpecRouted.h"
#include "gpopt/base/COrderSpec.h"
#include "gpopt/operators/CPhysicalMotion.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalMotionRoutedDistribute
//
//	@doc:
//		Routed distribute motion operator
//
//---------------------------------------------------------------------------
class CPhysicalMotionRoutedDistribute : public CPhysicalMotion
{
private:
	// routed distribution spec
	CDistributionSpecRouted *m_pdsRouted;

	// required columns in distribution spec
	CColRefSet *m_pcrsRequiredLocal;

public:
	CPhysicalMotionRoutedDistribute(const CPhysicalMotionRoutedDistribute &) =
		delete;

	// ctor
	CPhysicalMotionRoutedDistribute(CMemoryPool *mp,
									CDistributionSpecRouted *pdsRouted);

	// dtor
	~CPhysicalMotionRoutedDistribute() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalMotionRoutedDistribute;
	}

	const CHAR *
	SzId() const override
	{
		return "CPhysicalMotionRoutedDistribute";
	}

	// output distribution accessor
	CDistributionSpec *
	Pds() const override
	{
		return m_pdsRouted;
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
	static CPhysicalMotionRoutedDistribute *PopConvert(COperator *pop);

};	// class CPhysicalMotionRoutedDistribute

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalMotionRoutedDistribute_H

// EOF

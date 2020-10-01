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

	// private copy ctor
	CPhysicalMotionRoutedDistribute(const CPhysicalMotionRoutedDistribute &);

public:
	// ctor
	CPhysicalMotionRoutedDistribute(CMemoryPool *mp,
									CDistributionSpecRouted *pdsRouted);

	// dtor
	virtual ~CPhysicalMotionRoutedDistribute();

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopPhysicalMotionRoutedDistribute;
	}

	virtual const CHAR *
	SzId() const
	{
		return "CPhysicalMotionRoutedDistribute";
	}

	// output distribution accessor
	virtual CDistributionSpec *
	Pds() const
	{
		return m_pdsRouted;
	}

	// match function
	virtual BOOL Matches(COperator *) const;

	//-------------------------------------------------------------------------------------
	// Required Plan Properties
	//-------------------------------------------------------------------------------------

	// compute required output columns of the n-th child
	virtual CColRefSet *PcrsRequired(CMemoryPool *mp,
									 CExpressionHandle &exprhdl,
									 CColRefSet *pcrsInput, ULONG child_index,
									 CDrvdPropArray *pdrgpdpCtxt,
									 ULONG ulOptReq);

	// compute required sort order of the n-th child
	virtual COrderSpec *PosRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
									COrderSpec *posInput, ULONG child_index,
									CDrvdPropArray *pdrgpdpCtxt,
									ULONG ulOptReq) const;

	// check if required columns are included in output columns
	virtual BOOL FProvidesReqdCols(CExpressionHandle &exprhdl,
								   CColRefSet *pcrsRequired,
								   ULONG ulOptReq) const;

	//-------------------------------------------------------------------------------------
	// Derived Plan Properties
	//-------------------------------------------------------------------------------------

	// derive sort order
	virtual COrderSpec *PosDerive(CMemoryPool *mp,
								  CExpressionHandle &exprhdl) const;

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	//-------------------------------------------------------------------------------------

	// return order property enforcing type for this operator
	virtual CEnfdProp::EPropEnforcingType EpetOrder(
		CExpressionHandle &exprhdl, const CEnfdOrder *peo) const;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// print
	virtual IOstream &OsPrint(IOstream &) const;

	// conversion function
	static CPhysicalMotionRoutedDistribute *PopConvert(COperator *pop);

};	// class CPhysicalMotionRoutedDistribute

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalMotionRoutedDistribute_H

// EOF

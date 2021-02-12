// Greenplum Database
// Copyright (C) 2012 EMC Corp.
//
// Assert operator for runtime checking of constraints. Assert operators have a list
// of constraints to be checked, and corresponding error messages to print in the
// event of constraint violation.
//
// For example:
// clang-format off
//
// +--CPhysicalAssert (Error code: 23514)   
//    |--CPhysicalAssert (Error code: 23502)  
//    |  |--CPhysical [...]
//    |  +--CScalarAssertConstraintList
//    |     +--CScalarAssertConstraint (ErrorMsg: Not null constraint for column b of table r violated)
//    |        +--CScalarBoolOp (EboolopNot)
//    |           +--CScalarNullTest
//    |              +--CScalarIdent "b" (2) 
//    +--CScalarAssertConstraintList
//       |--CScalarAssertConstraint (ErrorMsg: Check constraint r_check for table r violated)
//       |  +--CScalarIsDistinctFrom (=)
//       |     |--CScalarCmp (<)
//       |     |  |--CScalarIdent "d" (4)
//       |     |  +--CScalarIdent "c" (3)
//       |     +--CScalarConst (0)
//       +--CScalarAssertConstraint (ErrorMsg: Check constraint r_c_check for table r violated)
//          +--CScalarIsDistinctFrom (=)
//             |--CScalarCmp (>)
//             |  |--CScalarIdent "c" (3) 
//             |  +--CScalarConst (0)
//             +--CScalarConst (0)
// clang-format on
#ifndef GPOPT_CPhysicalAssert_H
#define GPOPT_CPhysicalAssert_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysical.h"
#include "naucrates/dxl/errorcodes.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalAssert
//
//	@doc:
//		Assert operator
//
//---------------------------------------------------------------------------
class CPhysicalAssert : public CPhysical
{
private:
	// exception
	CException *m_pexc;


public:
	CPhysicalAssert(const CPhysicalAssert &) = delete;

	// ctor
	CPhysicalAssert(CMemoryPool *mp, CException *pexc);

	// dtor
	~CPhysicalAssert() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalAssert;
	}

	// operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalAssert";
	}

	// exception
	CException *
	Pexc() const
	{
		return m_pexc;
	}

	// match function
	BOOL Matches(COperator *pop) const override;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return true;
	}

	//-------------------------------------------------------------------------------------
	// Required Plan Properties
	//-------------------------------------------------------------------------------------

	// compute required output columns of the n-th child
	CColRefSet *PcrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							 CColRefSet *pcrsRequired, ULONG child_index,
							 CDrvdPropArray *pdrgpdpCtxt,
							 ULONG ulOptReq) override;

	// compute required ctes of the n-th child
	CCTEReq *PcteRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
						  CCTEReq *pcter, ULONG child_index,
						  CDrvdPropArray *pdrgpdpCtxt,
						  ULONG ulOptReq) const override;

	// compute required sort order of the n-th child
	COrderSpec *PosRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							COrderSpec *posRequired, ULONG child_index,
							CDrvdPropArray *pdrgpdpCtxt,
							ULONG ulOptReq) const override;

	// compute required distribution of the n-th child
	CDistributionSpec *PdsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
								   CDistributionSpec *pdsRequired,
								   ULONG child_index,
								   CDrvdPropArray *pdrgpdpCtxt,
								   ULONG ulOptReq) const override;

	// compute required rewindability of the n-th child
	CRewindabilitySpec *PrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
									CRewindabilitySpec *prsRequired,
									ULONG child_index,
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

	// derive distribution
	CDistributionSpec *PdsDerive(CMemoryPool *mp,
								 CExpressionHandle &exprhdl) const override;

	// derive rewindability
	CRewindabilitySpec *PrsDerive(CMemoryPool *mp,
								  CExpressionHandle &exprhdl) const override;

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	//-------------------------------------------------------------------------------------

	// return order property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetOrder(
		CExpressionHandle &exprhdl, const CEnfdOrder *peo) const override;

	// return rewindability property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetRewindability(
		CExpressionHandle &exprhdl,
		const CEnfdRewindability *per) const override;

	// return true if operator passes through stats obtained from children,
	// this is used when computing stats during costing
	BOOL
	FPassThruStats() const override
	{
		return true;
	}

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CPhysicalAssert *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopPhysicalAssert == pop->Eopid());

		return dynamic_cast<CPhysicalAssert *>(pop);
	}

	// debug print
	IOstream &OsPrint(IOstream &os) const override;

};	// class CPhysicalAssert

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalAssert_H

// EOF

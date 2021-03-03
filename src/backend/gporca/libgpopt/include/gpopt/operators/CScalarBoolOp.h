//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CScalarBoolOp.h
//
//	@doc:
//		Base class for all scalar boolean operators
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarBoolOp_H
#define GPOPT_CScalarBoolOp_H

#include "gpos/base.h"

#include "gpopt/base/CDrvdProp.h"
#include "gpopt/operators/CScalar.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CScalarBoolOp
//
//	@doc:
//		Scalar boolean operator
//
//---------------------------------------------------------------------------
class CScalarBoolOp : public CScalar
{
public:
	// enum of boolean operators
	enum EBoolOperator
	{
		EboolopAnd,	 // AND
		EboolopOr,	 // OR
		EboolopNot,	 // NOT

		EboolopSentinel
	};

private:
	static const WCHAR m_rgwszBool[EboolopSentinel][30];

	// boolean operator
	EBoolOperator m_eboolop;

public:
	CScalarBoolOp(const CScalarBoolOp &) = delete;

	// ctor
	CScalarBoolOp(CMemoryPool *mp, EBoolOperator eboolop)
		: CScalar(mp), m_eboolop(eboolop)
	{
		GPOS_ASSERT(0 <= eboolop && EboolopSentinel > eboolop);
	}

	// dtor
	~CScalarBoolOp() override = default;


	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopScalarBoolOp;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CScalarBoolOp";
	}

	// accessor
	EBoolOperator
	Eboolop() const
	{
		return m_eboolop;
	}

	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *) const override;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return !FCommutative(Eboolop());
	}

	// return a copy of the operator with remapped columns
	COperator *
	PopCopyWithRemappedColumns(CMemoryPool *,		//mp,
							   UlongToColRefMap *,	//colref_mapping,
							   BOOL					//must_exist
							   ) override
	{
		return PopCopyDefault();
	}

	// conversion function
	static CScalarBoolOp *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopScalarBoolOp == pop->Eopid());

		return dynamic_cast<CScalarBoolOp *>(pop);
	}

	// boolean expression evaluation
	EBoolEvalResult Eber(ULongPtrArray *pdrgpulChildren) const override;

	// decide boolean operator commutativity
	static BOOL FCommutative(EBoolOperator eboolop);

	// the type of the scalar expression
	IMDId *MdidType() const override;

	// print
	IOstream &OsPrint(IOstream &os) const override;


};	// class CScalarBoolOp

}  // namespace gpopt


#endif	// !GPOPT_CScalarBoolOp_H

// EOF

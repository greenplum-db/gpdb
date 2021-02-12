//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CScalarConst.h
//
//	@doc:
//		An operator class that wraps a scalar constant
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarConst_H
#define GPOPT_CScalarConst_H

#include "gpos/base.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CDrvdProp.h"
#include "gpopt/operators/CScalar.h"
#include "naucrates/base/IDatum.h"

namespace gpopt
{
using namespace gpos;
using namespace gpnaucrates;

//---------------------------------------------------------------------------
//	@class:
//		CScalarConst
//
//	@doc:
//		A wrapper operator for scalar constants
//
//---------------------------------------------------------------------------
class CScalarConst : public CScalar
{
private:
	// constant
	IDatum *m_pdatum;

public:
	CScalarConst(const CScalarConst &) = delete;

	// ctor
	CScalarConst(CMemoryPool *mp, IDatum *datum);

	// dtor
	~CScalarConst() override;

	// identity accessor
	EOperatorId
	Eopid() const override
	{
		return EopScalarConst;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CScalarConst";
	}

	// accessor of contained constant
	IDatum *
	GetDatum() const
	{
		return m_pdatum;
	}

	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *pop) const override;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return false;
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
	static CScalarConst *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopScalarConst == pop->Eopid());

		return dynamic_cast<CScalarConst *>(pop);
	}

	// the type of the scalar expression
	IMDId *MdidType() const override;

	INT TypeModifier() const override;

	// boolean expression evaluation
	EBoolEvalResult Eber(ULongPtrArray *pdrgpulChildren) const override;

	// print
	IOstream &OsPrint(IOstream &) const override;

	// is the given expression a scalar cast of a constant
	static BOOL FCastedConst(CExpression *pexpr);

	// extract the constant from the given constant expression or a casted constant expression.
	// Else return NULL.
	static CScalarConst *PopExtractFromConstOrCastConst(CExpression *pexpr);

};	// class CScalarConst

}  // namespace gpopt


#endif	// !GPOPT_CScalarConst_H

// EOF

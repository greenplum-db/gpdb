//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CScalarIf.h
//
//	@doc:
//		Scalar if operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarIf_H
#define GPOPT_CScalarIf_H

#include "gpos/base.h"

#include "gpopt/base/CDrvdProp.h"
#include "gpopt/operators/CScalar.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CScalarIf
//
//	@doc:
//		Scalar if operator. A case statement in SQL is represented as as
//		cascaded if statements. The format of if statement is:
//				if ------ condition
//				|-------- true value
//				|-------- false value
//		For example: (case when r.a < r.b then 10 when r.a > r.b then 20 else 15 end)
//		Is represented as if ---- r.a < r.b
//						   |----- 10
//						   |----- if ----- r.a > r.b
//								  |------- 20
//								  |------- 15
//
//---------------------------------------------------------------------------
class CScalarIf : public CScalar
{
private:
	// metadata id in the catalog
	IMDId *m_mdid_type;

	// is operator return type BOOL?
	BOOL m_fBoolReturnType;

public:
	CScalarIf(const CScalarIf &) = delete;

	// ctor
	CScalarIf(CMemoryPool *mp, IMDId *mdid);

	// dtor
	~CScalarIf() override
	{
		m_mdid_type->Release();
	}


	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopScalarIf;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CScalarIf";
	}

	// the type of the scalar expression
	IMDId *
	MdidType() const override
	{
		return m_mdid_type;
	}

	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *) const override;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return true;
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


	// boolean expression evaluation
	EBoolEvalResult
	Eber(ULongPtrArray *pdrgpulChildren) const override
	{
		return EberNullOnAllNullChildren(pdrgpulChildren);
	}

	// conversion function
	static CScalarIf *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopScalarIf == pop->Eopid());

		return dynamic_cast<CScalarIf *>(pop);
	}

};	// class CScalarIf

}  // namespace gpopt


#endif	// !GPOPT_CScalarIf_H

// EOF

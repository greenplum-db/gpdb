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
#include "gpopt/base/COptCtxt.h"
#include "gpopt/operators/CScalar.h"
#include "gpopt/base/CDrvdProp.h"

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

	// private copy ctor
	CScalarIf(const CScalarIf &);

public:
	// ctor
	CScalarIf(CMemoryPool *mp, IMDId *mdid);

	// dtor
	virtual ~CScalarIf()
	{
		m_mdid_type->Release();
	}


	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopScalarIf;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CScalarIf";
	}

	// the type of the scalar expression
	virtual IMDId *
	MdidType() const
	{
		return m_mdid_type;
	}

	// operator specific hash function
	virtual ULONG HashValue() const;

	// match function
	virtual BOOL Matches(COperator *) const;

	// sensitivity to order of inputs
	virtual BOOL
	FInputOrderSensitive() const
	{
		return true;
	}

	// return a copy of the operator with remapped columns
	virtual COperator *
	PopCopyWithRemappedColumns(CMemoryPool *,		//mp,
							   UlongToColRefMap *,	//colref_mapping,
							   BOOL					//must_exist
	)
	{
		return PopCopyDefault();
	}


	// boolean expression evaluation
	virtual EBoolEvalResult
	Eber(ULongPtrArray *pdrgpulChildren) const
	{
		return EberNullOnAllNullChildren(pdrgpulChildren);
	}

	// conversion function
	static CScalarIf *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopScalarIf == pop->Eopid());

		return dynamic_cast<CScalarIf *>(pop);
	}

};	// class CScalarIf

}  // namespace gpopt


#endif	// !GPOPT_CScalarIf_H

// EOF

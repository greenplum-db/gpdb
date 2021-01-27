//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CScalarSwitchCase.h
//
//	@doc:
//		Scalar SwitchCase operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarSwitchCase_H
#define GPOPT_CScalarSwitchCase_H

#include "gpos/base.h"

#include "gpopt/base/CDrvdProp.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/operators/CScalar.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CScalarSwitchCase
//
//	@doc:
//		Scalar SwitchCase operator
//
//---------------------------------------------------------------------------
class CScalarSwitchCase : public CScalar
{
private:
	// private copy ctor
	CScalarSwitchCase(const CScalarSwitchCase &);

public:
	// ctor
	explicit CScalarSwitchCase(CMemoryPool *mp);

	// dtor
	virtual ~CScalarSwitchCase()
	{
	}

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopScalarSwitchCase;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CScalarSwitchCase";
	}

	// match function
	virtual BOOL Matches(COperator *pop) const;

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

	virtual IMDId *
	MdidType() const
	{
		GPOS_ASSERT(!"Invalid function call: CScalarSwitchCase::MdidType()");
		return NULL;
	}

	// boolean expression evaluation
	virtual EBoolEvalResult
	Eber(ULongPtrArray *pdrgpulChildren) const
	{
		return EberNullOnAllNullChildren(pdrgpulChildren);
	}

	// conversion function
	static CScalarSwitchCase *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopScalarSwitchCase == pop->Eopid());

		return dynamic_cast<CScalarSwitchCase *>(pop);
	}

};	// class CScalarSwitchCase

}  // namespace gpopt


#endif	// !GPOPT_CScalarSwitchCase_H

// EOF

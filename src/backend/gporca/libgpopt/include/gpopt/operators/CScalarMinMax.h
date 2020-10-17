//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal Inc.
//
//	@filename:
//		CScalarMinMax.h
//
//	@doc:
//		Scalar MinMax operator
//
//		This returns the minimum (or maximum) value from a list of any number of
//		scalar expressions. NULL values in the list are ignored. The result will
//		be NULL only if all the expressions evaluate to NULL.
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarMinMax_H
#define GPOPT_CScalarMinMax_H

#include "gpos/base.h"
#include "gpopt/operators/CScalar.h"
#include "gpopt/base/CDrvdProp.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CScalarMinMax
//
//	@doc:
//		Scalar MinMax operator
//
//---------------------------------------------------------------------------
class CScalarMinMax : public CScalar
{
public:
	// types of operations: either min or max
	enum EScalarMinMaxType
	{
		EsmmtMin,
		EsmmtMax,
		EsmmtSentinel
	};

private:
	// return type
	IMDId *m_mdid_type;

	// min/max type
	EScalarMinMaxType m_esmmt;

	// is operator return type BOOL?
	BOOL m_fBoolReturnType;

	// private copy ctor
	CScalarMinMax(const CScalarMinMax &);

public:
	// ctor
	CScalarMinMax(CMemoryPool *mp, IMDId *mdid_type, EScalarMinMaxType esmmt);

	// dtor
	virtual ~CScalarMinMax();

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopScalarMinMax;
	}

	// operator name
	virtual const CHAR *
	SzId() const
	{
		return "CScalarMinMax";
	}

	// return type
	virtual IMDId *
	MdidType() const
	{
		return m_mdid_type;
	}

	// min/max type
	EScalarMinMaxType
	Esmmt() const
	{
		return m_esmmt;
	}

	// operator specific hash function
	virtual ULONG HashValue() const;

	// match function
	virtual BOOL Matches(COperator *pop) const;

	// sensitivity to order of inputs
	virtual BOOL
	FInputOrderSensitive() const
	{
		return false;
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
		// MinMax returns Null only if all children are Null
		return EberNullOnAllNullChildren(pdrgpulChildren);
	}

	// print
	virtual IOstream &OsPrint(IOstream &os) const;

	// conversion function
	static CScalarMinMax *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopScalarMinMax == pop->Eopid());

		return dynamic_cast<CScalarMinMax *>(pop);
	}

};	// class CScalarMinMax

}  // namespace gpopt

#endif	// !GPOPT_CScalarMinMax_H

// EOF

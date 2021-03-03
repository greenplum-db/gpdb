//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CScalarIsDistinctFrom.h
//
//	@doc:
//		Is distinct from operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarIsDistinctFrom_H
#define GPOPT_CScalarIsDistinctFrom_H

#include "gpos/base.h"

#include "gpopt/operators/CScalarCmp.h"


namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CScalarIsDistinctFrom
//
//	@doc:
//		Is distinct from operator
//
//---------------------------------------------------------------------------
class CScalarIsDistinctFrom : public CScalarCmp
{
private:
public:
	CScalarIsDistinctFrom(const CScalarIsDistinctFrom &) = delete;

	// ctor
	CScalarIsDistinctFrom(CMemoryPool *mp, IMDId *mdid_op,
						  const CWStringConst *pstrOp)
		: CScalarCmp(mp, mdid_op, pstrOp, IMDType::EcmptIDF)
	{
		GPOS_ASSERT(mdid_op->IsValid());
	}

	// dtor
	~CScalarIsDistinctFrom() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopScalarIsDistinctFrom;
	}

	// boolean expression evaluation
	EBoolEvalResult Eber(ULongPtrArray *pdrgpulChildren) const override;

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CScalarIsDistinctFrom";
	}

	BOOL Matches(COperator *pop) const override;

	// conversion function
	static CScalarIsDistinctFrom *PopConvert(COperator *pop);

	// get commuted scalar IDF operator
	CScalarIsDistinctFrom *PopCommutedOp(CMemoryPool *mp,
										 COperator *pop) override;

};	// class CScalarIsDistinctFrom

}  // namespace gpopt

#endif	// !GPOPT_CScalarIsDistinctFrom_H

// EOF

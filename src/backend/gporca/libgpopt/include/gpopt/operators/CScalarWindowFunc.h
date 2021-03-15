//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CScalarWindowFunc.h
//
//	@doc:
//		Class for scalar window function
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarWindowFunc_H
#define GPOPT_CScalarWindowFunc_H

#include "gpos/base.h"

#include "gpopt/base/CDrvdProp.h"
#include "gpopt/operators/CScalarFunc.h"
#include "naucrates/md/IMDId.h"

namespace gpopt
{
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CScalarWindowFunc
//
//	@doc:
//		Class for scalar window function
//
//---------------------------------------------------------------------------
class CScalarWindowFunc : public CScalarFunc
{
public:
	// window stage
	enum EWinStage
	{
		EwsImmediate,
		EwsPreliminary,
		EwsRowKey,

		EwsSentinel
	};

private:
	// window stage
	EWinStage m_ewinstage;

	// distinct window computation
	BOOL m_is_distinct;

	/* TRUE if argument list was really '*' */
	BOOL m_is_star_arg;

	/* is function a simple aggregate? */
	BOOL m_is_simple_agg;

	// aggregate window function, e.g. count(*) over()
	BOOL m_fAgg;

public:
	CScalarWindowFunc(const CScalarWindowFunc &) = delete;

	// ctor
	CScalarWindowFunc(CMemoryPool *mp, IMDId *mdid_func,
					  IMDId *mdid_return_type, const CWStringConst *pstrFunc,
					  EWinStage ewinstage, BOOL is_distinct, BOOL is_star_arg,
					  BOOL is_simple_agg);

	// dtor
	~CScalarWindowFunc() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopScalarWindowFunc;
	}

	// return a string for window function
	const CHAR *
	SzId() const override
	{
		return "CScalarWindowFunc";
	}

	EWinStage
	Ews() const
	{
		return m_ewinstage;
	}

	// operator specific hash function
	ULONG HashValue() const override;

	// match function
	BOOL Matches(COperator *pop) const override;

	// conversion function
	static CScalarWindowFunc *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopScalarWindowFunc == pop->Eopid());

		return dynamic_cast<CScalarWindowFunc *>(pop);
	}

	// does window function definition include Distinct?
	BOOL
	IsDistinct() const
	{
		return m_is_distinct;
	}

	BOOL
	IsStarArg() const
	{
		return m_is_star_arg;
	}

	BOOL
	IsSimpleAgg() const
	{
		return m_is_simple_agg;
	}

	// is window function defined as Aggregate?
	BOOL
	FAgg() const
	{
		return m_fAgg;
	}

	// print
	IOstream &OsPrint(IOstream &os) const override;


};	// class CScalarWindowFunc

}  // namespace gpopt

#endif	// !GPOPT_CScalarWindowFunc_H

// EOF

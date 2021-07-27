//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CLogicalLeftAntiSemiJoinNotIn.h
//
//	@doc:
//		Left anti semi join operator with the NotIn semantics
//			1 not in (2,3) --> true
//			1 not in (1,2,3) --> false
//			1 not in (null, 2) --> unknown
//			1 not in (1, null, 2) --> false
//			null not in (1,2) --> unknown
//			null not in (empty) --> true
//			null not in (1,2,null) --> unknown
//---------------------------------------------------------------------------
#ifndef GPOS_CLogicalLeftAntiSemiJoinNotIn_H
#define GPOS_CLogicalLeftAntiSemiJoinNotIn_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalLeftAntiSemiJoin.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalLeftAntiSemiJoinNotIn
//
//	@doc:
//		Left anti semi join operator with the NotIn semantics
//
//---------------------------------------------------------------------------
class CLogicalLeftAntiSemiJoinNotIn : public CLogicalLeftAntiSemiJoin
{
private:
public:
	CLogicalLeftAntiSemiJoinNotIn(const CLogicalLeftAntiSemiJoinNotIn &) =
		delete;

	// ctor
	explicit CLogicalLeftAntiSemiJoinNotIn(
		CMemoryPool *mp, CXform::EXformId origin_xform = CXform::ExfSentinel);

	// dtor
	~CLogicalLeftAntiSemiJoinNotIn() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalLeftAntiSemiJoinNotIn;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalLeftAntiSemiJoinNotIn";
	}

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalLeftAntiSemiJoinNotIn *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalLeftAntiSemiJoinNotIn == pop->Eopid());

		return dynamic_cast<CLogicalLeftAntiSemiJoinNotIn *>(pop);
	}

};	// class CLogicalLeftAntiSemiJoinNotIn

}  // namespace gpopt


#endif	// !GPOS_CLogicalLeftAntiSemiJoinNotIn_H

// EOF

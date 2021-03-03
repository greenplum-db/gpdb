//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CPatternMultiLeaf.h
//
//	@doc:
//		Pattern that matches a variable number of leaves
//---------------------------------------------------------------------------
#ifndef GPOPT_CPatternMultiLeaf_H
#define GPOPT_CPatternMultiLeaf_H

#include "gpos/base.h"

#include "gpopt/operators/CPattern.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CPatternMultiLeaf
//
//	@doc:
//		Pattern that matches a variable number of expressions, eg inputs to
//		union operator
//
//---------------------------------------------------------------------------
class CPatternMultiLeaf : public CPattern
{
private:
public:
	CPatternMultiLeaf(const CPatternMultiLeaf &) = delete;

	// ctor
	explicit CPatternMultiLeaf(CMemoryPool *mp) : CPattern(mp)
	{
	}

	// dtor
	~CPatternMultiLeaf() override = default;

	// check if operator is a pattern leaf
	BOOL
	FLeaf() const override
	{
		return true;
	}

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPatternMultiLeaf;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPatternMultiLeaf";
	}

};	// class CPatternMultiLeaf

}  // namespace gpopt


#endif	// !GPOPT_CPatternMultiLeaf_H

// EOF

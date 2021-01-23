//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CPatternTree.h
//
//	@doc:
//		Pattern that matches entire expression trees
//---------------------------------------------------------------------------
#ifndef GPOPT_CPatternTree_H
#define GPOPT_CPatternTree_H

#include "gpos/base.h"

#include "gpopt/operators/CPattern.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CPatternTree
//
//	@doc:
//		Pattern that matches entire expression trees, e.g. scalar expressions
//
//---------------------------------------------------------------------------
class CPatternTree : public CPattern
{
private:
public:
	CPatternTree(const CPatternTree &) = delete;

	// ctor
	explicit CPatternTree(CMemoryPool *mp) : CPattern(mp)
	{
	}

	// dtor
	~CPatternTree() override = default;

	// check if operator is a pattern leaf
	BOOL
	FLeaf() const override
	{
		return false;
	}

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPatternTree;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPatternTree";
	}

};	// class CPatternTree

}  // namespace gpopt


#endif	// !GPOPT_CPatternTree_H

// EOF

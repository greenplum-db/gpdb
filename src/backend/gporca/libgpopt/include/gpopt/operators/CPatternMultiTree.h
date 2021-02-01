//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CPatternMultiTree.h
//
//	@doc:
//		Pattern that matches a variable number of trees
//---------------------------------------------------------------------------
#ifndef GPOPT_CPatternMultiTree_H
#define GPOPT_CPatternMultiTree_H

#include "gpos/base.h"

#include "gpopt/operators/CPattern.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CPatternMultiTree
//
//	@doc:
//		Pattern that matches a variable number of trees
//
//---------------------------------------------------------------------------
class CPatternMultiTree : public CPattern
{
private:
public:
	CPatternMultiTree(const CPatternMultiTree &) = delete;

	// ctor
	explicit CPatternMultiTree(CMemoryPool *mp) : CPattern(mp)
	{
	}

	// dtor
	~CPatternMultiTree() override = default;

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
		return EopPatternMultiTree;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPatternMultiTree";
	}

};	// class CPatternMultiTree

}  // namespace gpopt


#endif	// !GPOPT_CPatternMultiTree_H

// EOF

//---------------------------------------------------------------------------
// Greenplum Database
// Copyright (c) 2020 VMware and affiliates, Inc.
//
// Transform a join into a bitmap index apply. Allow a variety of nodes on
// the inner side, including a mandatory get, plus optional select,
// project and aggregate nodes.
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformJoin2BitmapIndexGetApply_H
#define GPOPT_CXformJoin2BitmapIndexGetApply_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalJoin.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/operators/CPatternNode.h"
#include "gpopt/xforms/CXformJoin2IndexApplyGeneric.h"

namespace gpopt
{
using namespace gpos;

class CXformJoin2BitmapIndexGetApply : public CXformJoin2IndexApplyGeneric
{
private:
	// no copy ctor
	CXformJoin2BitmapIndexGetApply(const CXformJoin2BitmapIndexGetApply &);

public:
	// ctor
	explicit CXformJoin2BitmapIndexGetApply(CMemoryPool *mp)
		: CXformJoin2IndexApplyGeneric(mp, true)
	{
	}

	// dtor
	virtual ~CXformJoin2BitmapIndexGetApply()
	{
	}

	// ident accessors
	virtual EXformId
	Exfid() const
	{
		return ExfJoin2BitmapIndexGetApply;
	}

	virtual const CHAR *
	SzId() const
	{
		return "CXformJoin2BitmapIndexGetApply";
	}

};	// class CXformJoin2BitmapIndexGetApply

}  // namespace gpopt

#endif	// !GPOPT_CXformJoin2BitmapIndexGetApply_H

// EOF

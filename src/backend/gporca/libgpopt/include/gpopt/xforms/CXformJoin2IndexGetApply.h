//---------------------------------------------------------------------------
// Greenplum Database
// Copyright (c) 2020 VMware and affiliates, Inc.
//
// Transform a join into a non-bitmap index apply. Allow a variety of nodes on
// the inner side, including a mandatory get, plus optional select,
// project and aggregate nodes.
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformJoin2IndexGetApply_H
#define GPOPT_CXformJoin2IndexGetApply_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalJoin.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/operators/CPatternNode.h"
#include "gpopt/xforms/CXformJoin2IndexApplyGeneric.h"

namespace gpopt
{
using namespace gpos;

class CXformJoin2IndexGetApply : public CXformJoin2IndexApplyGeneric
{
private:
	// no copy ctor
	CXformJoin2IndexGetApply(const CXformJoin2IndexGetApply &);

public:
	// ctor
	explicit CXformJoin2IndexGetApply(CMemoryPool *mp)
		: CXformJoin2IndexApplyGeneric(mp, false)
	{
	}

	// dtor
	virtual ~CXformJoin2IndexGetApply()
	{
	}

	// ident accessors
	virtual EXformId
	Exfid() const
	{
		return ExfJoin2IndexGetApply;
	}

	virtual const CHAR *
	SzId() const
	{
		return "CXformJoin2IndexGetApply";
	}

};	// class CXformJoin2IndexGetApply

}  // namespace gpopt

#endif	// !GPOPT_CXformJoin2IndexGetApply_H

// EOF

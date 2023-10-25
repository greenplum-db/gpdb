//-------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformMinMax2IndexOnlyGet.h
//
//	@doc:
//		Transform aggregates min, max to queries with IndexOnlyScan with
//		Limit.
//-------------------------------------------------------------------
#ifndef GPOPT_CXformMinMax2IndexOnlyGet_H
#define GPOPT_CXformMinMax2IndexOnlyGet_H

#include "gpos/base.h"

#include "gpopt/operators/CLogical.h"
#include "gpopt/xforms/CXformMinMax2IndexGet.h"

namespace gpopt
{
using namespace gpos;

//-------------------------------------------------------------------
//	@class:
//		CXformMinMax2IndexOnlyGet
//
//	@doc:
//		Transform aggregates min, max to queries with IndexOnlyScan with
//		Limit.
//-------------------------------------------------------------------
class CXformMinMax2IndexOnlyGet : public CXformMinMax2IndexGet
{
public:
	CXformMinMax2IndexOnlyGet(const CXformMinMax2IndexOnlyGet &) = delete;

	// ctor
	CXformMinMax2IndexOnlyGet(CMemoryPool *mp) : CXformMinMax2IndexGet(mp)
	{
	}

	// dtor
	~CXformMinMax2IndexOnlyGet() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfMinMax2IndexOnlyGet;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformMinMax2IndexOnlyGet";
	}

};	// class CXformMinMax2IndexOnlyGet

}  // namespace gpopt


#endif	//GPOPT_CXformMinMax2IndexOnlyGet_H

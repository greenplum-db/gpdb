//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformLimit2IndexOnlyGet.h
//
//	@doc:
//		Transform LogicalGet in a limit to LogicalIndexOnlyGet if order by
//		columns any of the index prefix
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformLimit2IndexOnlyGet_H
#define GPOPT_CXformLimit2IndexOnlyGet_H

#include "gpos/base.h"

#include "gpopt/operators/CLogical.h"
#include "gpopt/xforms/CXformLimit2IndexGet.h"
namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformLimit2IndexOnlyGet
//
//	@doc:
//		Transform LogicalGet in a limit to LogicalIndexGet if order by columns
//		match any of the index prefix
//---------------------------------------------------------------------------
class CXformLimit2IndexOnlyGet : public CXformLimit2IndexGet
{
public:
	CXformLimit2IndexOnlyGet(const CXformLimit2IndexOnlyGet &) = delete;
	// ctor
	explicit CXformLimit2IndexOnlyGet(CMemoryPool *mp)
		: CXformLimit2IndexGet(mp)
	{
	}

	// dtor
	~CXformLimit2IndexOnlyGet() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfLimit2IndexOnlyGet;
	}

	// xform name
	const CHAR *
	SzId() const override
	{
		return "CXformLimit2IndexOnlyGet";
	}


};	// class CXformLimit2IndexOnlyGet

}  // namespace gpopt


#endif	//GPOPT_CXformLimit2IndexOnlyGet_H

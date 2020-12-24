//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformImplementPartitionSelector.h
//
//	@doc:
//		Implement PartitionSelector
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementPartitionSelector_H
#define GPOPT_CXformImplementPartitionSelector_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformImplementPartitionSelector
//
//	@doc:
//		Implement PartitionSelector
//
//---------------------------------------------------------------------------
class CXformImplementPartitionSelector : public CXformImplementation
{
private:
public:
	CXformImplementPartitionSelector(const CXformImplementPartitionSelector &) =
		delete;

	// ctor
	explicit CXformImplementPartitionSelector(CMemoryPool *mp);

	// dtor
	~CXformImplementPartitionSelector() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfImplementPartitionSelector;
	}

	// xform name
	const CHAR *
	SzId() const override
	{
		return "CXformImplementPartitionSelector";
	}

	// compute xform promise for a given expression handle
	EXformPromise
	Exfp(CExpressionHandle &  //exprhdl
	) const override
	{
		return CXform::ExfpHigh;
	}

	// actual transform
	void Transform(CXformContext *, CXformResult *,
				   CExpression *) const override;

};	// class CXformImplementPartitionSelector

}  // namespace gpopt

#endif	// !GPOPT_CXformImplementPartitionSelector_H

// EOF

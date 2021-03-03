//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CXformImplementation.h
//
//	@doc:
//		Base class for implementation transforms
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementation_H
#define GPOPT_CXformImplementation_H

#include "gpos/base.h"

#include "gpopt/xforms/CXform.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformImplementation
//
//	@doc:
//		base class for all implementations
//
//---------------------------------------------------------------------------
class CXformImplementation : public CXform
{
private:
public:
	CXformImplementation(const CXformImplementation &) = delete;

	// ctor
	explicit CXformImplementation(CExpression *);

	// dtor
	~CXformImplementation() override;

	// type of operator
	BOOL
	FImplementation() const override
	{
		GPOS_ASSERT(!FSubstitution() && !FExploration());
		return true;
	}

};	// class CXformImplementation

}  // namespace gpopt


#endif	// !GPOPT_CXformImplementation_H

// EOF

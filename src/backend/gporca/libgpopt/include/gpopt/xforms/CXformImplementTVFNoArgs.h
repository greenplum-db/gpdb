//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformImplementTVFNoArgs.h
//
//	@doc:
//		Implement logical TVF with a physical TVF with no arguments
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementTVFNoArgs_H
#define GPOPT_CXformImplementTVFNoArgs_H

#include "gpos/base.h"

#include "gpopt/xforms/CXformImplementTVF.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformImplementTVFNoArgs
//
//	@doc:
//		Implement TVF with no arguments
//
//---------------------------------------------------------------------------
class CXformImplementTVFNoArgs : public CXformImplementTVF
{
private:
public:
	CXformImplementTVFNoArgs(const CXformImplementTVFNoArgs &) = delete;

	// ctor
	explicit CXformImplementTVFNoArgs(CMemoryPool *mp);

	// dtor
	~CXformImplementTVFNoArgs() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfImplementTVFNoArgs;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformImplementTVFNoArgs";
	}

};	// class CXformImplementTVFNoArgs

}  // namespace gpopt

#endif	// !GPOPT_CXformImplementTVFNoArgs_H

// EOF

//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CXformPushGbDedupBelowJoin.h
//
//	@doc:
//		Push dedup group by below join transform
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformPushGbDedupBelowJoin_H
#define GPOPT_CXformPushGbDedupBelowJoin_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformPushGbBelowJoin.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformPushGbDedupBelowJoin
//
//	@doc:
//		Push dedup group by below join transform
//
//---------------------------------------------------------------------------
class CXformPushGbDedupBelowJoin : public CXformPushGbBelowJoin
{
private:
public:
	CXformPushGbDedupBelowJoin(const CXformPushGbDedupBelowJoin &) = delete;

	// ctor
	explicit CXformPushGbDedupBelowJoin(CMemoryPool *mp);

	// dtor
	~CXformPushGbDedupBelowJoin() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfPushGbDedupBelowJoin;
	}

	const CHAR *
	SzId() const override
	{
		return "CXformPushGbDedupBelowJoin";
	}

};	// class CXformPushGbDedupBelowJoin

}  // namespace gpopt

#endif	// !GPOPT_CXformPushGbDedupBelowJoin_H

// EOF

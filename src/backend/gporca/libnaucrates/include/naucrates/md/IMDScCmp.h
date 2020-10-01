//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		IMDScCmp.h
//
//	@doc:
//		Interface for scalar comparison operators in the MD cache
//---------------------------------------------------------------------------



#ifndef GPMD_IMDScCmp_H
#define GPMD_IMDScCmp_H

#include "gpos/base.h"

#include "naucrates/md/IMDCacheObject.h"
#include "naucrates/md/IMDType.h"

namespace gpmd
{
using namespace gpos;


//---------------------------------------------------------------------------
//	@class:
//		IMDScCmp
//
//	@doc:
//		Interface for scalar comparison operators in the MD cache
//
//---------------------------------------------------------------------------
class IMDScCmp : public IMDCacheObject
{
public:
	// object type
	virtual Emdtype
	MDType() const
	{
		return EmdtScCmp;
	}

	// left type
	virtual IMDId *GetLeftMdid() const = 0;

	// right type
	virtual IMDId *GetRightMdid() const = 0;

	// comparison type
	virtual IMDType::ECmpType ParseCmpType() const = 0;

	// comparison operator id
	virtual IMDId *MdIdOp() const = 0;
};

}  // namespace gpmd

#endif	// !GPMD_IMDScCmp_H

// EOF

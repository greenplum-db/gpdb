//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		IMDInterface.h
//
//	@doc:
//		Base interface for metadata-related objects
//---------------------------------------------------------------------------



#ifndef GPMD_IMDInterface_H
#define GPMD_IMDInterface_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"

namespace gpmd
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		IMDInterface
//
//	@doc:
//		Base interface for metadata-related objects
//
//---------------------------------------------------------------------------
class IMDInterface : public CRefCount
{
public:
	virtual ~IMDInterface()
	{
	}
};
}  // namespace gpmd



#endif	// !GPMD_IMDInterface_H

// EOF

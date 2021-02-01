//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CTableDescriptorTest.h
//
//	@doc:
//      Test for CTableDescriptor
//---------------------------------------------------------------------------
#ifndef GPOPT_CTableDescriptorTest_H
#define GPOPT_CTableDescriptorTest_H

#include "gpos/base.h"
#include "gpos/common/CDynamicPtrArray.h"

#include "gpopt/base/CColRef.h"
#include "gpopt/metadata/CColumnDescriptor.h"
#include "gpopt/metadata/CTableDescriptor.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CTableDescriptorTest
//
//	@doc:
//		Static unit tests
//
//---------------------------------------------------------------------------
class CTableDescriptorTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();
};	// class CTableDescriptorTest
}  // namespace gpopt

#endif	// !GPOPT_CTableDescriptorTest_H

// EOF

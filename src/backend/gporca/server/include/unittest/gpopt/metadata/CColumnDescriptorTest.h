//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CColumnDescriptorTest.h
//
//	@doc:
//		Test for CColumnDescriptor
//---------------------------------------------------------------------------
#ifndef GPOPT_CColumnDescriptorTest_H
#define GPOPT_CColumnDescriptorTest_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"

#include "gpopt/metadata/CName.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CColumnDescriptorTest
//
//	@doc:
//		Static unit tests
//
//---------------------------------------------------------------------------
class CColumnDescriptorTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();

};	// class CColumnDescriptorTest
}  // namespace gpopt

#endif	// !GPOPT_CColumnDescriptorTest_H

// EOF

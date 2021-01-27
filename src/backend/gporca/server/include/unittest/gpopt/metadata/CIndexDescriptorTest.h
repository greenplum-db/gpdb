//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CIndexDescriptorTest.h
//
//	@doc:
//      Test for CTableDescriptor
//---------------------------------------------------------------------------
#ifndef GPOPT_CIndexDescriptorTest_H
#define GPOPT_CIndexDescriptorTest_H

#include "gpos/base.h"

#include "gpopt/metadata/CIndexDescriptor.h"
#include "gpopt/metadata/CTableDescriptor.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CIndexDescriptorTest
//
//	@doc:
//		Static unit tests
//
//---------------------------------------------------------------------------
class CIndexDescriptorTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();
};	// class CIndexDescriptorTest
}  // namespace gpopt

#endif	// !GPOPT_CIndexDescriptorTest_H

// EOF

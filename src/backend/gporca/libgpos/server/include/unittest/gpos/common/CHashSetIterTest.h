//	Greenplum Database
//	Copyright (C) 2017 VMware, Inc. or its affiliates

#ifndef GPOS_CHashSetIterTest_H
#define GPOS_CHashSetIterTest_H

#include "gpos/base.h"

namespace gpos
{
// Static unit tests
class CHashSetIterTest
{
public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();

};	// class CHashSetIterTest
}  // namespace gpos

#endif	// !GPOS_CHashSetIterTest_H

// EOF

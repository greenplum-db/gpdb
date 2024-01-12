//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (c) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		IHint.h
//---------------------------------------------------------------------------
#ifndef GPOS_IHint_H
#define GPOS_IHint_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"

namespace gpopt
{
using namespace gpos;

class IHint : public CRefCount
{
};


}  // namespace gpopt

#endif	// !GPOS_IHint_H

// EOF

//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CDrvdProp.cpp
//
//	@doc:
//		Implementation of derived properties
//---------------------------------------------------------------------------

#include "gpopt/base/CDrvdProp.h"

#include "gpos/base.h"

#include "gpopt/operators/COperator.h"

#ifdef GPOS_DEBUG
#include "gpos/error/CAutoTrace.h"

#include "gpopt/base/COptCtxt.h"
#endif	// GPOS_DEBUG

FORCE_GENERATE_DBGSTR(gpopt::CDrvdProp);

namespace gpopt
{
CDrvdProp::CDrvdProp() = default;

IOstream &
operator<<(IOstream &os, const CDrvdProp &drvdprop)
{
	return drvdprop.OsPrint(os);
}

}  // namespace gpopt

// EOF

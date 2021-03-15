//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 VMware, Inc. or its affiliates.
//
//	@filename:
//		CEnfdProp.cpp
//
//	@doc:
//		Implementation of enforced property
//---------------------------------------------------------------------------

#include "gpopt/base/CEnfdProp.h"

#include "gpos/base.h"

#ifdef GPOS_DEBUG
#include "gpos/error/CAutoTrace.h"

#include "gpopt/base/COptCtxt.h"
#endif	// GPOS_DEBUG

FORCE_GENERATE_DBGSTR(gpopt::CEnfdProp);

namespace gpopt
{
IOstream &
operator<<(IOstream &os, CEnfdProp &efdprop)
{
	return efdprop.OsPrint(os);
}

}  // namespace gpopt

// EOF

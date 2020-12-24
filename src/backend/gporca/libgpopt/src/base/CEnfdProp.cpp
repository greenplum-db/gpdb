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

#include "gpos/base.h"

#include "gpopt/base/CEnfdProp.h"

#ifdef GPOS_DEBUG
#include "gpopt/base/COptCtxt.h"
#include "gpos/error/CAutoTrace.h"
#endif	// GPOS_DEBUG

namespace gpopt
{
IOstream &
operator<<(IOstream &os, CEnfdProp &efdprop)
{
	return efdprop.OsPrint(os);
}

}  // namespace gpopt

// EOF

//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 Pivotal Inc.
//
//	@filename:
//		base.h
//
//	@doc:
//		Global definitions for optimizer unittests
//---------------------------------------------------------------------------
#ifndef UNITTEST_BASE_H
#define UNITTEST_BASE_H

#include "gpos/base.h"
#include "gpos/common/CAutoP.h"
#include "gpos/common/CAutoRef.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/test/CUnittest.h"

#include "gpdbcost/CCostModelGPDB.h"
#include "gpopt/base/CAutoOptCtxt.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/mdcache/CMDCache.h"
#include "naucrates/md/CMDProviderMemory.h"

// number of segments for running tests
#define GPOPT_TEST_SEGMENTS 2

using namespace gpopt;
using namespace gpmd;
using namespace gpdxl;
using namespace gpdbcost;

#endif	// UNITTEST_BASE_H

// EOF

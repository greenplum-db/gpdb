//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		types.h
//
//	@doc:
//		clib definitions for GPOS;
//---------------------------------------------------------------------------
#ifndef GPOS_clibtypes_H
#define GPOS_clibtypes_H

#include <dlfcn.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>

#include "gpos/types.h"

namespace gpos
{
// container for user and system time
using RUSAGE = struct rusage;

// represent an elapsed time
using TIMEVAL = struct timeval;

// hold minimal information about the local time zone
using TIMEZONE = struct timezone;

// represents an elapsed time
using TIMESPEC = struct timespec;

// store system time values
using TIME_T = time_t;

// containing a calendar date and time broken down into its components.
using TIME = struct tm;

// store information of a calling process
using DL_INFO = Dl_info;
}  // namespace gpos

#endif	// !GPOS_clibtypes_H

// EOF

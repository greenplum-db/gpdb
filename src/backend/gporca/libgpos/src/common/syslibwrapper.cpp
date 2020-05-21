//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		syslibwrapper.cpp
//
//	@doc:
//		Wrapper for functions in system library
//
//---------------------------------------------------------------------------

#include <syslog.h>

#include <sys/time.h>
#include <sys/stat.h>

#include "gpos/assert.h"
#include "gpos/utils.h"

#include "gpos/common/syslibwrapper.h"
#include "gpos/error/CException.h"


using namespace gpos;


//---------------------------------------------------------------------------
//	@function:
//		syslib::GetTimeOfDay
//
//	@doc:
//		Get the date and time
//
//---------------------------------------------------------------------------
void
gpos::syslib::GetTimeOfDay
	(
	TIMEVAL *tv,
	TIMEZONE *tz
	)
{
	GPOS_ASSERT(NULL != tv);

#ifdef GPOS_DEBUG
	INT res =
#endif // GPOS_DEBUG
	gettimeofday(tv, tz);

	GPOS_ASSERT(0 == res);
}


//---------------------------------------------------------------------------
//	@function:
//		syslib::GetRusage
//
//	@doc:
//		Get system and user time
//
//---------------------------------------------------------------------------
void
gpos::syslib::GetRusage
	(
	RUSAGE *usage
	)
{
	GPOS_ASSERT(NULL != usage);

#ifdef GPOS_DEBUG
	INT res =
#endif // GPOS_DEBUG
	getrusage(RUSAGE_SELF, usage);

	GPOS_ASSERT(0 == res);
}


//---------------------------------------------------------------------------
//	@function:
//		syslib::OpenLog
//
//	@doc:
//		Open a connection to the system logger for a program
//
//---------------------------------------------------------------------------
void
gpos::syslib::OpenLog
	(
	const CHAR *ident,
	INT option,
	INT facility
	)
{
	openlog(ident, option, facility);
}


//---------------------------------------------------------------------------
//	@function:
//		syslib::SysLog
//
//	@doc:
//		Generate a log message
//
//---------------------------------------------------------------------------
void
gpos::syslib::SysLog
	(
	INT priority,
	const CHAR *format
	)
{
	syslog(priority, "%s", format);
}


//---------------------------------------------------------------------------
//	@function:
//		syslib::CloseLog
//
//	@doc:
//		Close the descriptor being used to write to the system logger
//
//---------------------------------------------------------------------------
void
gpos::syslib::CloseLog
	(
	)
{
	closelog();
}

// EOF


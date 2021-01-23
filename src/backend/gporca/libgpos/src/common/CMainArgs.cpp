//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 - 2010 Greenplum, Inc.
//
//	@filename:
//		CMainArgs.cpp
//
//	@doc:
//		Implementation of getopt abstraction
//---------------------------------------------------------------------------

#include "gpos/common/CMainArgs.h"

#include "gpos/base.h"
#include "gpos/common/clibwrapper.h"

using namespace gpos;


//---------------------------------------------------------------------------
//	@function:
//		CMainArgs::CMainArgs
//
//	@doc:
//		ctor -- saves off all opt params
//
//---------------------------------------------------------------------------
CMainArgs::CMainArgs(ULONG argc, const CHAR **argv, const CHAR *fmt)
	: m_argc(argc),
	  m_argv(argv),
	  m_fmt(fmt),
	  m_optarg(optarg),
	  m_optind(optind),
	  m_optopt(optopt),
	  m_opterr(opterr)
#ifdef GPOS_Darwin
	  ,
	  m_optreset(optreset)
#endif	// GPOS_Darwin
{
	// initialize external opt params
	optarg = NULL;
	optind = 1;
	optopt = 1;
	opterr = 1;
#ifdef GPOS_Darwin
	optreset = 1;
#endif	// GPOS_Darwin
}


//---------------------------------------------------------------------------
//	@function:
//		CMainArgs::~CMainArgs
//
//	@doc:
//		dtor -- restore previous opt params
//
//---------------------------------------------------------------------------
CMainArgs::~CMainArgs()
{
	optarg = m_optarg;
	optind = m_optind;
	optopt = m_optopt;
	opterr = m_opterr;
#ifdef GPOS_Darwin
	optreset = m_optreset;
#endif	// GPOS_Darwin
}


//---------------------------------------------------------------------------
//	@function:
//		CMainArgs::Getopt
//
//	@doc:
//		wraps getopt logic
//
//---------------------------------------------------------------------------
BOOL
CMainArgs::Getopt(CHAR *pch)
{
	GPOS_ASSERT(NULL != pch);

	INT res = clib::Getopt(m_argc, const_cast<CHAR **>(m_argv), m_fmt);

	if (res != -1)
	{
		*pch = (CHAR) res;
		return true;
	}

	return false;
}

// EOF

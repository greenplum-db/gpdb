//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CMappingColIdVar.cpp
//
//	@doc:
//		Implementation of base ColId-> Var mapping class
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "gpopt/translate/CMappingColIdVar.h"

using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CMappingColIdVar::CMappingColIdVar
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CMappingColIdVar::CMappingColIdVar(CMemoryPool *mp) : m_mp(mp)
{
}

// EOF

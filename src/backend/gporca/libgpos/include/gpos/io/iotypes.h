//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		iotypes.h
//
//	@doc:
//		IO type definitions for GPOS;
//---------------------------------------------------------------------------
#ifndef GPOS_iotypes_H
#define GPOS_iotypes_H

#include <errno.h>
#include <sys/stat.h>


namespace gpos
{
// file state structure
typedef struct stat SFileStat;
}  // namespace gpos

#endif	// !GPOS_iotypes_H

// EOF

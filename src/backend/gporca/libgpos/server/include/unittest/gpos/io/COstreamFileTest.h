//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		COstreamFileTest.h
//
//	@doc:
//		Test for COstreamFile
//---------------------------------------------------------------------------
#ifndef GPOS_COstreamFileTest_H
#define GPOS_COstreamFileTest_H

#include "gpos/base.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		COstreamFileTest
//
//	@doc:
//		Static unit tests for messages
//
//---------------------------------------------------------------------------
class COstreamFileTest
{
private:
	// write to output file stream
	static void Unittest_WriteFileStream(const CHAR *szFile);

	// check file stream for correctness
	static void Unittest_CheckOutputFile(const CHAR *szFile);

	// delete temporary file and containing directory
	static void Unittest_DeleteTmpFile(const CHAR *szDir, const CHAR *szFile);

public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_Basic();
};
}  // namespace gpos

#endif	// !GPOS_COstreamFileTest_H

// EOF

//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CFileTest.h
//
//	@doc:
//		Tests for CFileWriter and CFileReader
//---------------------------------------------------------------------------

#ifndef GPOS_CFileTest_H
#define GPOS_CFileTest_H

#include "gpos/base.h"
#include "gpos/io/ioutils.h"
#include "gpos/io/CFileWriter.h"
#include "gpos/io/CFileReader.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CFileTest
//
//	@doc:
//		Static unit tests for CFileWriter and CFileReader
//
//---------------------------------------------------------------------------
class CFileTest
{
public:
	// help CFileWriter get file size by descriptor
	class CFileWriterInternal : public CFileWriter
	{
	public:
		// get file size by descriptor
		ULLONG UllSizeInternal() const;

		// dtor
		virtual ~CFileWriterInternal();


	};	// class CFileWriterInternal

	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_FileContent();
	static GPOS_RESULT EresUnittest_Invalid();
	static GPOS_RESULT EresUnittest_InconsistentSize();

	static void Unittest_MkTmpFile(CHAR *szTmpDir, CHAR *szTmpFile);
	static void Unittest_DeleteTmpDir(const CHAR *szDir, const CHAR *szFile);

	template <typename T, typename R, typename ARG1, typename ARG2>
	static void Unittest_CheckError(T *pt, R (T::*pfunc)(ARG1, ARG2),
									ARG1 argFirst, ARG2 argSec,
									CException::ExMinor exmi);

	static void Unittest_WriteInconsistentSize(const CHAR *szTmpFile);
	static void Unittest_ReadInconsistentSize(const CHAR *szTmpFile);

};	// class CFileTest

}  // namespace gpos

#endif	// !GPOS_CFileTest_H

// EOF

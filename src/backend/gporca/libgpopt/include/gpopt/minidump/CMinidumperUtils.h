//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CMinidumperUtils.h
//
//	@doc:
//		Minidump utility functions
//---------------------------------------------------------------------------
#ifndef GPOPT_CMiniDumperUtils_H
#define GPOPT_CMiniDumperUtils_H

#include "gpos/base.h"
#include "gpos/error/CMiniDumper.h"

#include "gpopt/minidump/CDXLMinidump.h"

using namespace gpos;

namespace gpopt
{
// fwd decl
class ICostModel;
class CMiniDumperDXL;
class COptimizerConfig;
class IConstExprEvaluator;

//---------------------------------------------------------------------------
//	@class:
//		CMinidumperUtils
//
//	@doc:
//		Minidump utility functions
//
//---------------------------------------------------------------------------
class CMinidumperUtils
{
public:
	// load a minidump
	static CDXLMinidump *PdxlmdLoad(CMemoryPool *mp, const CHAR *file_name);

	// generate a minidump file name in the provided buffer
	static void GenerateMinidumpFileName(CHAR *buf, ULONG length,
										 ULONG ulSessionId, ULONG ulCmdId,
										 const CHAR *szMinidumpFileName = NULL);

	// finalize minidump and dump to a file
	static void Finalize(CMiniDumperDXL *pmdp, BOOL fSerializeErrCtx);

	// load and execute the minidump in the specified file
	static CDXLNode *PdxlnExecuteMinidump(CMemoryPool *mp,
										  const CHAR *file_name,
										  ULONG ulSegments, ULONG ulSessionId,
										  ULONG ulCmdId,
										  COptimizerConfig *optimizer_config,
										  IConstExprEvaluator *pceeval = NULL);

	// execute the given minidump
	static CDXLNode *PdxlnExecuteMinidump(CMemoryPool *mp,
										  CDXLMinidump *pdxlmdp,
										  const CHAR *file_name,
										  ULONG ulSegments, ULONG ulSessionId,
										  ULONG ulCmdId,
										  COptimizerConfig *optimizer_config,
										  IConstExprEvaluator *pceeval = NULL);

	// execute the given minidump using the given MD accessor
	static CDXLNode *PdxlnExecuteMinidump(
		CMemoryPool *mp, CMDAccessor *md_accessor, CDXLMinidump *pdxlmd,
		const CHAR *file_name, ULONG ulSegments, ULONG ulSessionId,
		ULONG ulCmdId, COptimizerConfig *optimizer_config,
		IConstExprEvaluator *pceeval);

};	// class CMinidumperUtils

}  // namespace gpopt

#endif	// !GPOPT_CMiniDumperUtils_H

// EOF

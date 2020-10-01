//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		exception.h
//
//	@doc:
//		Definition of DXL-specific exception types
//---------------------------------------------------------------------------
#ifndef DXL_exception_H
#define DXL_exception_H

#include "gpos/types.h"
#include "gpos/memory/CMemoryPool.h"

namespace gpdxl
{
// major exception types - reserve range 200-1000
enum ExMajor
{
	ExmaDXL = 200,
	ExmaMD = 300,
	ExmaComm = 400,
	ExmaGPDB = 500,
	ExmaConstExprEval = 600,

	ExmaSentinel
};

// minor exception types
enum ExMinor
{
	// DXL-parsing related errors
	ExmiDXLUnexpectedTag,
	ExmiDXLMissingAttribute,
	ExmiDXLInvalidAttributeValue,
	ExmiDXLUnrecognizedOperator,
	ExmiDXLUnrecognizedType,
	ExmiDXLUnrecognizedCompOperator,
	ExmiDXLValidationError,
	ExmiDXLXercesParseError,
	ExmiDXLIncorrectNumberOfChildren,
	ExmiPlStmt2DXLConversion,
	ExmiDXL2PlStmtConversion,
	ExmiDXL2PlStmtExternalScanError,
	ExmiDXL2PlStmtMissingPlanForSubPlanTranslation,
	ExmiQuery2DXLAttributeNotFound,
	ExmiQuery2DXLUnsupportedFeature,
	ExmiQuery2DXLDuplicateRTE,
	ExmiQuery2DXLMissingValue,
	ExmiQuery2DXLNotNullViolation,
	ExmiQuery2DXLError,
	ExmiExpr2DXLUnsupportedFeature,
	ExmiExpr2DXLAttributeNotFound,
	ExmiDXL2PlStmtAttributeNotFound,
	ExmiDXL2ExprAttributeNotFound,

	// MD related errors
	ExmiMDCacheEntryDuplicate,
	ExmiMDCacheEntryNotFound,
	ExmiMDObjUnsupported,

	// communication related errors
	ExmiCommPropagateError,
	ExmiCommUnexpectedMessage,

	// GPDB-related exceptions
	ExmiGPDBError,

	// exceptions related to constant expression evaluation
	ExmiConstExprEvalNonConst,

	// ORCA Exceptions that need to be reported as ERROR to GPDB
	ExmiOptimizerError,
	ExmiNoAvailableMemory,
	ExmiInvalidComparisonTypeCode,

	ExmiDXLSentinel
};

// message initialization for GPOS exceptions
gpos::GPOS_RESULT EresExceptionInit(gpos::CMemoryPool *mp);

}  // namespace gpdxl

#endif	// !DXL_exception_H


// EOF

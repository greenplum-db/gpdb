//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		exception.h
//
//	@doc:
//		Definition of GPOPT-specific exception types
//---------------------------------------------------------------------------
#ifndef GPOPT_exception_H
#define GPOPT_exception_H

#include "gpos/types.h"
#include "gpos/memory/CMemoryPool.h"

namespace gpopt
{
// major exception types - reserve range 1000-2000
enum ExMajor
{
	ExmaGPOPT = 1000,

	ExmaSentinel
};

// minor exception types
enum ExMinor
{
	ExmiNoPlanFound,
	ExmiInvalidPlanAlternative,
	ExmiUnsupportedOp,
	ExmiUnexpectedOp,
	ExmiUnsupportedPred,
	ExmiUnsupportedCompositePartKey,
	ExmiUnsupportedNonDeterministicUpdate,
	ExmiUnsatisfiedRequiredProperties,
	ExmiEvalUnsupportedScalarExpr,
	ExmiCTEProducerConsumerMisAligned,

	ExmiSentinel
};

// message initialization for GPOS exceptions
gpos::GPOS_RESULT EresExceptionInit(gpos::CMemoryPool *mp);

}  // namespace gpopt

#endif	// !GPOPT_exception_H


// EOF

//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		exception.cpp
//
//	@doc:
//		Initialization of GPOPT-specific exception messages
//---------------------------------------------------------------------------

#include "gpopt/exception.h"

#include "gpos/error/CMessage.h"
#include "gpos/error/CMessageRepository.h"

using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		EresExceptionInit
//
//	@doc:
//		Message initialization for GPOPT exceptions
//
//---------------------------------------------------------------------------
GPOS_RESULT
gpopt::EresExceptionInit(CMemoryPool *mp)
{
	//---------------------------------------------------------------------------
	// Basic DXL messages in English
	//---------------------------------------------------------------------------
	CMessage rgmsg[ExmiSentinel] = {
		CMessage(CException(gpopt::ExmaGPOPT, gpopt::ExmiNoPlanFound),
				 CException::ExsevError,
				 GPOS_WSZ_WSZLEN(
					 "No plan has been computed for required properties"),
				 0, GPOS_WSZ_WSZLEN("No plan found")),

		CMessage(
			CException(gpopt::ExmaGPOPT, gpopt::ExmiInvalidPlanAlternative),
			CException::ExsevError,
			GPOS_WSZ_WSZLEN(
				"Plan identifier %lld out of range, max plans: %lld"),
			2,	// plan id, max plans
			GPOS_WSZ_WSZLEN("Plan enumeration")),

		CMessage(CException(gpopt::ExmaGPOPT, gpopt::ExmiUnsupportedOp),
				 CException::ExsevNotice,
				 GPOS_WSZ_WSZLEN("Operator %ls not supported"),
				 1,	 // operator type
				 GPOS_WSZ_WSZLEN("Unsupported operator")),

		CMessage(CException(gpopt::ExmaGPOPT, gpopt::ExmiUnexpectedOp),
				 CException::ExsevError,
				 GPOS_WSZ_WSZLEN("Unexpected Operator %ls"),
				 1,	 // operator type
				 GPOS_WSZ_WSZLEN("Unexpected operator")),

		CMessage(CException(gpopt::ExmaGPOPT, gpopt::ExmiUnsupportedPred),
				 CException::ExsevNotice,
				 GPOS_WSZ_WSZLEN("Predicate %s not supported"),
				 1,	 // predicate type
				 GPOS_WSZ_WSZLEN("Unsupported predicate")),

		CMessage(
			CException(gpopt::ExmaGPOPT,
					   gpopt::ExmiUnsupportedCompositePartKey),
			CException::ExsevNotice,
			GPOS_WSZ_WSZLEN(
				"Feature not supported by the Pivotal Query Optimizer: composite partitioning keys"),
			0,
			GPOS_WSZ_WSZLEN(
				"Feature not supported by the Pivotal Query Optimizer: composite partitioning keys")),

		CMessage(
			CException(gpopt::ExmaGPOPT,
					   gpopt::ExmiUnsupportedNonDeterministicUpdate),
			CException::ExsevNotice,
			GPOS_WSZ_WSZLEN(
				"Feature not supported by the Pivotal Query Optimizer: non-deterministic DML statements"),
			0,
			GPOS_WSZ_WSZLEN(
				"Feature not supported by the Pivotal Query Optimizer: non-deterministic DML statements")),

		CMessage(CException(gpopt::ExmaGPOPT,
							gpopt::ExmiUnsatisfiedRequiredProperties),
				 CException::ExsevError,
				 GPOS_WSZ_WSZLEN("Plan does not satisfy required properties"),
				 0,
				 GPOS_WSZ_WSZLEN("Plan does not satisfy required properties")),

		CMessage(
			CException(gpopt::ExmaGPOPT, gpopt::ExmiEvalUnsupportedScalarExpr),
			CException::ExsevError,
			GPOS_WSZ_WSZLEN(
				"Expecting a scalar expression like (const cmp const), ignoring casts"),
			0, GPOS_WSZ_WSZLEN("Not a constant scalar expression")),

		CMessage(
			CException(gpopt::ExmaGPOPT,
					   gpopt::ExmiCTEProducerConsumerMisAligned),
			CException::ExsevError,
			GPOS_WSZ_WSZLEN(
				"CTE Producer-Consumer execution locality mismatch for CTE id %lld"),
			1,
			GPOS_WSZ_WSZLEN(
				"CTE Producer-Consumer execution locality mismatch")),
	};

	GPOS_RESULT eres = GPOS_FAILED;

	GPOS_TRY
	{
		// copy exception array into heap
		CMessage *rgpmsg[gpopt::ExmiSentinel];
		CMessageRepository *pmr = CMessageRepository::GetMessageRepository();

		for (ULONG i = 0; i < GPOS_ARRAY_SIZE(rgpmsg); i++)
		{
			rgpmsg[i] = GPOS_NEW(mp) CMessage(rgmsg[i]);
			pmr->AddMessage(ElocEnUS_Utf8, rgpmsg[i]);
		}

		eres = GPOS_OK;
	}
	GPOS_CATCH_EX(ex)
	{
		return GPOS_FAILED;
	}

	GPOS_CATCH_END;

	return eres;
}


// EOF

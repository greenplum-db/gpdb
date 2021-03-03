/*-------------------------------------------------------------------------
 *
 * assert.c
 *	  Assert code.
 *
 * Portions Copyright (c) 2005-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/error/assert.c
 *
 * NOTE
 *	  This should eventually work with elog()
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "libpq/pqsignal.h"
#include "cdb/cdbvars.h"                /* gp_reraise_signal */

#include <unistd.h>

/*
 * ExceptionalCondition - Handles the failure of an Assert()
 */
void
ExceptionalCondition(const char *conditionName,
					 const char *errorType,
					 const char *fileName,
					 int lineNumber)
{
    /* CDB: Try to tell the QD or client what happened. */
	if (!PointerIsValid(conditionName)
		|| !PointerIsValid(fileName)
		|| !PointerIsValid(errorType))
		ereport(FATAL,
				errFatalReturn(gp_reraise_signal),
				errmsg("TRAP: ExceptionalCondition: bad arguments"));
	else
		ereport(FATAL,
				errFatalReturn(gp_reraise_signal),
				errmsg("Unexpected internal error"),
				errdetail("%s(\"%s\", File: \"%s\", Line: %d)\n",
						  errorType, conditionName, fileName, lineNumber));
				
	/* Usually this shouldn't be needed, but make sure the msg went out */
	fflush(stderr);

#ifdef SLEEP_ON_ASSERT

	/*
	 * It would be nice to use pg_usleep() here, but only does 2000 sec or 33
	 * minutes, which seems too short.
	 */
	sleep(1000000);
#endif

	abort();
}

#include "postgres.h"

#include "funcapi.h"
#include "miscadmin.h"

#include "access/heapam.h"
#include "access/genam.h"
#include "catalog/indexing.h"
#include "cdb/cdbvars.h"
#include "libpq/ip.h"
#include "postmaster/postmaster.h"
#include "utils/builtins.h"
#include "utils/faultinjector.h"
#include "utils/fmgroids.h"
#include "utils/snapmgr.h"

PG_MODULE_MAGIC;

extern Datum gp_inject_fault(PG_FUNCTION_ARGS);

static char *
processTransitionRequest_faultInject(char *faultName, char *type, char *ddlStatement, char *databaseName, char *tableName, int numOccurrences, int sleepTimeSeconds)
{
	StringInfo buf = makeStringInfo();
#ifdef FAULT_INJECTOR
	FaultInjectorEntry_s    faultInjectorEntry;

	elog(DEBUG1, "FAULT INJECTED: Name %s Type %s, DDL %s, DB %s, Table %s, NumOccurrences %d  SleepTime %d",
		 faultName, type, ddlStatement, databaseName, tableName, numOccurrences, sleepTimeSeconds );

	strlcpy(faultInjectorEntry.faultName, faultName, sizeof(faultInjectorEntry.faultName));
	faultInjectorEntry.faultInjectorIdentifier = FaultInjectorIdentifierStringToEnum(faultName);
	if (faultInjectorEntry.faultInjectorIdentifier == FaultInjectorIdNotSpecified) {
		ereport(COMMERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("could not recognize fault name")));

		appendStringInfo(buf, "Failure: could not recognize fault name");
		goto exit;
	}

	faultInjectorEntry.faultInjectorType = FaultInjectorTypeStringToEnum(type);
	if (faultInjectorEntry.faultInjectorType == FaultInjectorTypeNotSpecified ||
		faultInjectorEntry.faultInjectorType == FaultInjectorTypeMax) {
		ereport(COMMERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("could not recognize fault type")));

		appendStringInfo(buf, "Failure: could not recognize fault type");
		goto exit;
	}

	faultInjectorEntry.sleepTime = sleepTimeSeconds;
	if (sleepTimeSeconds < 0 || sleepTimeSeconds > 7200) {
		ereport(COMMERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("invalid sleep time, allowed range [0, 7200 sec]")));

		appendStringInfo(buf, "Failure: invalid sleep time, allowed range [0, 7200 sec]");
		goto exit;
	}

	faultInjectorEntry.ddlStatement = FaultInjectorDDLStringToEnum(ddlStatement);
	if (faultInjectorEntry.ddlStatement == DDLMax) {
		ereport(COMMERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("could not recognize DDL statement")));

		appendStringInfo(buf, "Failure: could not recognize DDL statement");
		goto exit;
	}

	snprintf(faultInjectorEntry.databaseName, sizeof(faultInjectorEntry.databaseName), "%s", databaseName);

	snprintf(faultInjectorEntry.tableName, sizeof(faultInjectorEntry.tableName), "%s", tableName);

	faultInjectorEntry.occurrence = numOccurrences;
	if (numOccurrences > 1000)
	{
		ereport(COMMERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("invalid occurrence number, allowed range [1, 1000]")));

		appendStringInfo(buf, "Failure: invalid occurrence number, allowed range [1, 1000]");
		goto exit;
	}

	if (FaultInjector_SetFaultInjection(&faultInjectorEntry) == STATUS_OK)
	{
		if (faultInjectorEntry.faultInjectorType == FaultInjectorTypeStatus)
			appendStringInfo(buf, "%s", faultInjectorEntry.bufOutput);
		else
			appendStringInfo(buf, "Success:");
	}
	else
		appendStringInfo(buf, "Failure: %s", faultInjectorEntry.bufOutput);

exit:
#else
	appendStringInfo(buf, "Failure: Fault Injector not available");
#endif
	return buf->data;
}


PG_FUNCTION_INFO_V1(gp_inject_fault);
Datum
gp_inject_fault(PG_FUNCTION_ARGS)
{
	char	   *faultName = TextDatumGetCString(PG_GETARG_DATUM(0));
	char	   *type = TextDatumGetCString(PG_GETARG_DATUM(1));
	char	   *ddlStatement = TextDatumGetCString(PG_GETARG_DATUM(2));
	char	   *databaseName = TextDatumGetCString(PG_GETARG_DATUM(3));
	char	   *tableName = TextDatumGetCString(PG_GETARG_DATUM(4));
	int			numOccurrences = PG_GETARG_INT32(5);
	int			sleepTimeSeconds = PG_GETARG_INT32(6);
	int         dbid = PG_GETARG_INT32(7);

	/* Fast path if injecting fault in our postmaster. */
	if (GpIdentity.dbid == dbid)
	{
		char	   *response;

		response = processTransitionRequest_faultInject(
			faultName, type, ddlStatement, databaseName,
			tableName, numOccurrences, sleepTimeSeconds);
		if (!response)
			elog(ERROR, "failed to inject fault locally (dbid %d)", dbid);
		if (strncmp(response, "Success:",  strlen("Success:")) != 0)
			elog(ERROR, "%s", response);

		elog(NOTICE, "%s", response);
	}
	else if (Gp_role == GP_ROLE_DISPATCH)
	{
		/*
		 * Otherwise, relay the command to executor nodes.
		 *
		 * We'd only really need to dispatch it to the one that it's meant for,
		 * but for now, just send it everywhere. The other nodes will just
		 * ignore it.
		 *
		 * (Perhaps this function should be defined as EXECUTE ON SEGMENTS,
		 * instead of dispatching manually here? But then it wouldn't run on
		 * QD. There is no EXECUTE ON SEGMENTS AND MASTER options, at the
		 * moment...)
		 */
		char *sql;

		sql = psprintf("select gp_inject_fault(%s, %s, %s, %s, %s, %d, %d, %d)",
					   quote_literal_internal(faultName),
					   quote_literal_internal(type),
					   quote_literal_internal(ddlStatement),
					   quote_literal_internal(databaseName),
					   quote_literal_internal(tableName),
					   numOccurrences,
					   sleepTimeSeconds,
					   dbid);
	}
	PG_RETURN_DATUM(BoolGetDatum(true));
}

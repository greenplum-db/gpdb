/*
 * An little helper function that provides SQL-level access to the fault
 * injection mechanism in the server.
 */
#include "postgres.h"

#include "funcapi.h"
#include "utils/builtins.h"
#include "utils/faultinjector.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

Datum gp_inject_fault(PG_FUNCTION_ARGS);

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
	FaultInjectorEntry_s	faultInjectorEntry;
	char buf[1000];

	elog(DEBUG1, "FAULT INJECTED: Name %s Type %s, DDL %s, DB %s, Table %s, NumOccurrences %d  SleepTime %d",
		 faultName, type, ddlStatement, databaseName, tableName, numOccurrences, sleepTimeSeconds );

	faultInjectorEntry.faultInjectorIdentifier = FaultInjectorIdentifierStringToEnum(faultName);
	if (faultInjectorEntry.faultInjectorIdentifier == FaultInjectorIdNotSpecified ||
		faultInjectorEntry.faultInjectorIdentifier == FaultInjectorIdMax)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("could not recognize fault name")));

	faultInjectorEntry.faultInjectorType = FaultInjectorTypeStringToEnum(type);
	if (faultInjectorEntry.faultInjectorType == FaultInjectorTypeNotSpecified ||
		faultInjectorEntry.faultInjectorType == FaultInjectorTypeMax)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("could not recognize fault type")));

	faultInjectorEntry.sleepTime = sleepTimeSeconds;
	if (sleepTimeSeconds < 0 || sleepTimeSeconds > 7200)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							errmsg("invalid sleep time, allowed range [0, 7200 sec]")));

	faultInjectorEntry.ddlStatement = FaultInjectorDDLStringToEnum(ddlStatement);
	if (faultInjectorEntry.ddlStatement == DDLMax)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("could not recognize DDL statement")));

	snprintf(faultInjectorEntry.databaseName, sizeof(faultInjectorEntry.databaseName), "%s", databaseName);

	snprintf(faultInjectorEntry.tableName, sizeof(faultInjectorEntry.tableName), "%s", tableName);

	faultInjectorEntry.occurrence = numOccurrences;
	if (numOccurrences > 1000)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid occurrence number, allowed range [1, 1000]")));

	if (FaultInjector_SetFaultInjection(&faultInjectorEntry) == STATUS_OK)
	{
		if (faultInjectorEntry.faultInjectorType == FaultInjectorTypeStatus)
			snprintf(buf, sizeof(buf), "%s", faultInjectorEntry.bufOutput);
		else
			snprintf(buf, sizeof(buf), "Success:");
	}
	else
		snprintf(buf, sizeof(buf), "Failure: %s", faultInjectorEntry.bufOutput);

	PG_RETURN_DATUM(CStringGetTextDatum(buf));
}

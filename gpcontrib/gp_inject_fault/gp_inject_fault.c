#include "postgres.h"

#include "funcapi.h"
#include "miscadmin.h"

#include "access/heapam.h"
#include "access/genam.h"
#include "catalog/indexing.h"
#include "cdb/cdbdisp_query.h"
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
processTransitionRequest_faultInject(char *faultName, char *type, char *ddlStatement, char *databaseName, char *tableName, int startOccurrence, int endOccurrence, int extraArg)
{
	StringInfo buf = makeStringInfo();
#ifdef FAULT_INJECTOR
	FaultInjectorEntry_s    faultInjectorEntry;

	elog(DEBUG1, "FAULT INJECTED: Name %s Type %s, DDL %s, DB %s, Table %s, StartOccurrence %d, EndOccurrence %d, extraArg %d",
		 faultName, type, ddlStatement, databaseName, tableName, startOccurrence, endOccurrence, extraArg );

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

	faultInjectorEntry.extraArg = extraArg;

	if (faultInjectorEntry.faultInjectorType == FaultInjectorTypeSleep)
	{
		if (extraArg < 0 || extraArg > 7200) {
			ereport(COMMERROR,
					(errcode(ERRCODE_PROTOCOL_VIOLATION),
					 errmsg("invalid sleep time, allowed range [0, 7200 sec]")));

			appendStringInfo(buf, "Failure: invalid sleep time, allowed range [0, 7200 sec]");
			goto exit;
		}
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

	if (startOccurrence < 1 || startOccurrence > 1000)
	{
		ereport(COMMERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("invalid start occurrence number, allowed range [1, 1000]")));

		appendStringInfo(buf, "Failure: invalid occurrence number, allowed range [1, 1000]");
		goto exit;
	}

	if (endOccurrence != INFINITE_END_OCCURRENCE && endOccurrence < startOccurrence)
	{
		ereport(COMMERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("invalid end occurrence number, allowed range [startOccurrence, ] or -1")));

		appendStringInfo(buf, "Failure: invalid end occurrence number, allowed range [startOccurrence, ] or -1");
		goto exit;
	}

	faultInjectorEntry.startOccurrence = startOccurrence;
	faultInjectorEntry.endOccurrence = endOccurrence;


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
	char	*faultName = TextDatumGetCString(PG_GETARG_DATUM(0));
	char	*type = TextDatumGetCString(PG_GETARG_DATUM(1));
	char	*ddlStatement = TextDatumGetCString(PG_GETARG_DATUM(2));
	char	*databaseName = TextDatumGetCString(PG_GETARG_DATUM(3));
	char	*tableName = TextDatumGetCString(PG_GETARG_DATUM(4));
	int		startOccurrence = PG_GETARG_INT32(5);
	int		endOccurrence = PG_GETARG_INT32(6);
	int		extraArg = PG_GETARG_INT32(7);
	int		dbid = PG_GETARG_INT32(8);

	/* Fast path if injecting fault in our postmaster. */
	if (GpIdentity.dbid == dbid)
	{
		char	   *response;

		response = processTransitionRequest_faultInject(
			faultName, type, ddlStatement, databaseName,
			tableName, startOccurrence, endOccurrence, extraArg);
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
		 *
		 * NOTE: Because we use the normal dispatcher to send this query,
		 * if a fault has already been injected to the dispatcher code,
		 * this will trigger it. That means that if you wish to inject
		 * faults on both the dispatcher and an executor in the same test,
		 * you need to be careful with the order you inject the faults!
		 */
		char	   *sql;

		sql = psprintf("select gp_inject_fault(%s, %s, %s, %s, %s, %d, %d, %d, %d)",
					   quote_literal_cstr(faultName),
					   quote_literal_cstr(type),
					   quote_literal_cstr(ddlStatement),
					   quote_literal_cstr(databaseName),
					   quote_literal_cstr(tableName),
					   startOccurrence,
					   endOccurrence,
					   extraArg,
					   dbid);

		CdbDispatchCommand(sql, DF_CANCEL_ON_ERROR, NULL);
	}
	PG_RETURN_DATUM(BoolGetDatum(true));
}

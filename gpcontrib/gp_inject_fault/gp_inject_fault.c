#include "postgres.h"

#include "funcapi.h"
#include "miscadmin.h"

#include "access/heapam.h"
#include "access/genam.h"
#include "catalog/indexing.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbvars.h"
#include "libpq/ip.h"
#include "libpq-fe.h"
#include "postmaster/postmaster.h"
#include "postmaster/fts.h"
#include "utils/builtins.h"
#include "utils/faultinjector.h"
#include "utils/fmgroids.h"
#include "utils/snapmgr.h"

PG_MODULE_MAGIC;

extern Datum gp_inject_fault(PG_FUNCTION_ARGS);
extern Datum gp_inject_fault2(PG_FUNCTION_ARGS);
void _PG_init(void);

static void
fts_with_panic_warning(FaultInjectorEntry_s faultEntry)
{
	if (Gp_role == GP_ROLE_DISPATCH && !FtsIsActive()) return;

	if (faultEntry.faultInjectorType == FaultInjectorTypePanic)
		ereport(WARNING, (
			errmsg("consider disabling FTS probes while injecting a panic."),
				errhint("Inject an infinite 'skip' into the 'fts_probe' fault to disable FTS probing.")));
}

/*
 * Register warning when extension is loaded.
 *
 */
void
_PG_init(void)
{
	InjectFaultInit();

	MemoryContext oldContext = MemoryContextSwitchTo(TopMemoryContext);
	register_fault_injection_warning(fts_with_panic_warning);
	MemoryContextSwitchTo(oldContext);
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

		response = InjectFault(
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

PG_FUNCTION_INFO_V1(gp_inject_fault2);
Datum
gp_inject_fault2(PG_FUNCTION_ARGS)
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
	char	*hostname = TextDatumGetCString(PG_GETARG_DATUM(9));
	int		port = PG_GETARG_INT32(10);
	char	*response;

	/* Fast path if injecting fault in our postmaster. */
	if (GpIdentity.dbid == dbid)
	{
		response = InjectFault(
			faultName, type, ddlStatement, databaseName,
			tableName, startOccurrence, endOccurrence, extraArg);
		if (!response)
			elog(ERROR, "failed to inject fault locally (dbid %d)", dbid);
		if (strncmp(response, "Success:",  strlen("Success:")) != 0)
			elog(ERROR, "%s", response);
	}
	else
	{
		char conninfo[1024];
		char msg[1024];
		PGconn *conn;
		PGresult *res;

		snprintf(conninfo, 1024, "host=%s port=%d %s=%s",
				 hostname, port, GPCONN_TYPE, GPCONN_TYPE_FAULT);
		conn = PQconnectdb(conninfo);
		if (PQstatus(conn) != CONNECTION_OK)
			elog(ERROR, "connection to dbid %d %s:%d failed", dbid, hostname, port);

		/*
		 * If ddl, dbname or tablename is not specified, send '#' instead.
		 * This allows sscanf to be used on the receiving end to parse the
		 * message.
		 */
		if (!ddlStatement || ddlStatement[0] == '\0')
			ddlStatement = "#";
		if (!databaseName || databaseName[0] == '\0')
			databaseName = "#";
		if (!tableName || tableName[0] == '\0')
			tableName = "#";
		snprintf(msg, 1024, "faultname=%s type=%s ddl=%s db=%s table=%s "
				 "start=%d end=%d extra=%d",
				 faultName, type,
				 ddlStatement,
				 databaseName,
				 tableName,
				 startOccurrence,
				 endOccurrence,
				 extraArg);
		res = PQexec(conn, msg);
		if (PQresultStatus(res) != PGRES_TUPLES_OK)
			elog(ERROR, "failed to inject fault: %s", PQerrorMessage(conn));

		if (PQntuples(res) != 1)
		{
			PQclear(res);
			PQfinish(conn);
			elog(ERROR, "invalid response from %s:%d", hostname, port);
		}

		response = PQgetvalue(res, 0, Anum_fault_message_response_status);
		if (strncmp(response, "Success:",  strlen("Success:")) != 0)
		{
			PQclear(res);
			PQfinish(conn);
			elog(ERROR, "%s", response);
		}

		PQclear(res);
		PQfinish(conn);
	}
	PG_RETURN_TEXT_P(cstring_to_text(response));
}

#include "postgres.h"
#include "funcapi.h"
#include "miscadmin.h"

#include "access/heapam.h"
#include "access/genam.h"
#include "cdb/cdbvars.h"
#include "libpq/ip.h"
#include "postmaster/postmaster.h"
#include "postmaster/primary_mirror_transition_client.h"
#include "utils/builtins.h"
#include "utils/faultinjector.h"
#include "utils/fmgroids.h"
#include "utils/snapmgr.h"

PG_MODULE_MAGIC;

extern Datum gp_inject_fault(PG_FUNCTION_ARGS);

static StringInfo transitionMsgErrors;

static void
transitionErrorLogFn(char *response)
{
	appendStringInfo(transitionMsgErrors, "%s\n", response);
}

static void
transitionReceivedDataFn(char *response)
{
	elog(NOTICE, "%s", response);
}

static bool
checkForNeedToExitFn(void)
{
	CHECK_FOR_INTERRUPTS();
	return false;
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
	StringInfo  faultmsg = makeStringInfo();

	/* Fast path if injecting fault in our postmaster. */
	if (GpIdentity.dbid == dbid)
	{
		appendStringInfo(faultmsg, "%s\n%s\n%s\n%s\n%s\n%d\n%d\n",
						 faultName, type, ddlStatement, databaseName,
						 tableName, numOccurrences, sleepTimeSeconds);
		int offset = 0;
		char *response =
			processTransitionRequest_faultInject(
				faultmsg->data, &offset, faultmsg->len);
		if (!response)
			elog(ERROR, "failed to inject fault locally (dbid %d)", dbid);
		if (strncmp(response, "Success:",  strlen("Success:")) != 0)
			elog(ERROR, "%s", response);

		elog(NOTICE, "%s", response);
		PG_RETURN_DATUM(true);
	}

	/* Obtain host and port of the requested dbid */
	HeapTuple tuple;
	Relation rel = heap_open(GpSegmentConfigRelationId, AccessShareLock);
	ScanKeyData scankey;
	SysScanDesc sscan;
	ScanKeyInit(&scankey,
				Anum_gp_segment_configuration_dbid,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum((int16) dbid));
	sscan = systable_beginscan(rel, GpSegmentConfigDbidIndexId, true,
							   GetTransactionSnapshot(), 1, &scankey);
	tuple = systable_getnext(sscan);

	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cannot find dbid %d", dbid);

	bool isnull;
	Datum datum = heap_getattr(tuple, Anum_gp_segment_configuration_hostname,
							   RelationGetDescr(rel), &isnull);
	char *hostname;
	if (!isnull)
		hostname =
				DatumGetCString(DirectFunctionCall1(textout, datum));
	else
		elog(ERROR, "hostname is null for dbid %d", dbid);
	int port = DatumGetInt32(heap_getattr(tuple,
										  Anum_gp_segment_configuration_port,
										  RelationGetDescr(rel), &isnull));
	systable_endscan(sscan);
	heap_close(rel, NoLock);

	struct addrinfo *addrList = NULL;
	struct addrinfo hint;
	int			ret;

	/* Initialize hint structure */
	MemSet(&hint, 0, sizeof(hint));
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_family = AF_UNSPEC;

	char portStr[100];
	if (snprintf(portStr, sizeof(portStr), "%d", port) >= sizeof(portStr))
		elog(ERROR, "port number too long for dbid %d", dbid);

	/* Use pg_getaddrinfo_all() to resolve the address */
	ret = pg_getaddrinfo_all(hostname, portStr, &hint, &addrList);
	if (ret || !addrList)
	{
		if (addrList)
			pg_freeaddrinfo_all(hint.ai_family, addrList);
		elog(ERROR, "could not translate host name \"%s\" to address: %s\n",
			 hostname, gai_strerror(ret));
	}

	PrimaryMirrorTransitionClientInfo client;
	client.receivedDataCallbackFn = transitionReceivedDataFn;
	client.errorLogFn = transitionErrorLogFn;
	client.checkForNeedToExitFn = checkForNeedToExitFn;
	transitionMsgErrors = makeStringInfo();

	appendStringInfo(faultmsg, "%s\n%s\n%s\n%s\n%s\n%s\n%d\n%d\n",
					 "faultInject",	faultName, type, ddlStatement,
					 databaseName, tableName, numOccurrences,
					 sleepTimeSeconds);

	if (sendTransitionMessage(&client, addrList, faultmsg->data, faultmsg->len,
							  1 /* retries */, 60 /* timeout */) !=
		TRANS_ERRCODE_SUCCESS)
	{
		pg_freeaddrinfo_all(hint.ai_family, addrList);
		ereport(ERROR, (errmsg("failed to inject %s fault in dbid %d",
							   faultName, dbid),
						errdetail("%s", transitionMsgErrors->data)));
	}

	pg_freeaddrinfo_all(hint.ai_family, addrList);
	PG_RETURN_DATUM(BoolGetDatum(true));
}

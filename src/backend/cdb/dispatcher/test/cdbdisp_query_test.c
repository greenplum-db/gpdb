#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../cdbdisp_query.c"

/*
 * Mocked object initializations required for cdbdisp_buildPlanQueryParms.
 */
void
_init_cdbdisp_buildPlanQueryParms(QueryDesc *queryDesc)
{
	queryDesc->estate = (struct EState *)palloc0(sizeof(struct EState));
	queryDesc->estate->es_sliceTable =
		(struct SliceTable *) palloc0(sizeof(struct SliceTable));
	queryDesc->operation = CMD_NOTHING;
	queryDesc->plannedstmt = (PlannedStmt *)palloc0(sizeof(PlannedStmt));
	queryDesc->plannedstmt->planTree = (struct Plan *)palloc0(sizeof(struct Plan));

	expect_any(RootSliceIndex, estate);
	will_return(RootSliceIndex, 0);
}

bool
verify_built_query_parms(DispatchCommandQueryParms *pQueryParms)
{
	if (pQueryParms == NULL)
		return false;

	if (strcmp(pQueryParms->strCommand, "SELECT * FROM foo") == 0 &&
		pQueryParms->serializedQuerytree == NULL &&
		pQueryParms->serializedQuerytreelen == 0 &&
		strcmp(pQueryParms->serializedPlantree, "serialized plan") == 0 &&
		pQueryParms->serializedPlantreelen == 3072 &&
		pQueryParms->serializedParams == NULL &&
		pQueryParms->serializedParamslen == 0 &&
		strcmp(pQueryParms->serializedQueryDispatchDesc, "serialized dispatch desc") == 0 &&
		pQueryParms->serializedQueryDispatchDesclen == 1024 &&
		pQueryParms->rootIdx == 0 &&
		strcmp(pQueryParms->seqServerHost, "127.0.0.1") == 0 &&
		pQueryParms->seqServerHostlen == 10 &&
		strcmp(pQueryParms->serializedDtxContextInfo, "serialized dtx context info") == 0 &&
		pQueryParms->serializedDtxContextInfolen == 512 &&
		pQueryParms->seqServerPort == 1234)
		return true;

	return false;
}

/*
 * Test that cdbdisp_dispatchPlan handles a plan size overflow well
 */
void
test__cdbdisp_buildPlanQueryParms__Overflow_plan_size_in_kb(void **state)
{
	bool success = false;

	struct QueryDesc *queryDesc = (struct QueryDesc *)
		palloc0(sizeof(QueryDesc));

	_init_cdbdisp_buildPlanQueryParms(queryDesc);

	/*
	 * Set max plan to a value that will require handling INT32
	 * overflow of the current plan size
	 */
	gp_max_plan_size = 1024;

	will_assign_value(serializeNode, uncompressed_size_out, INT_MAX-1);
	expect_any(serializeNode, node);
	expect_any(serializeNode, size);
	expect_any(serializeNode, uncompressed_size_out);

	will_return(serializeNode, NULL);

	PG_TRY();
	{
		cdbdisp_buildPlanQueryParms(queryDesc, false);
	}
	PG_CATCH();
	{
		/*
		 * Verify that we get the correct error (limit exceeded)
		 * CopyErrorData() requires us to get out of ErrorContext
		 */
		CurrentMemoryContext = TopMemoryContext;

		ErrorData *edata = CopyErrorData();

		StringInfo message = makeStringInfo();
		appendStringInfo(message,
						 "Query plan size limit exceeded, current size: " UINT64_FORMAT "KB, max allowed size: 1024KB",
						 ((INT_MAX-1)/(uint64)1024));

		if (edata->elevel == ERROR &&
			strncmp(edata->message, message->data, message->len) == 0)
		{
			success = true;
		}

	}
	PG_END_TRY();

	assert_true(success);
}

void
test__cdbdisp_buildPlanQueryParms__no_params(void **state)
{
	DispatchCommandQueryParms *pQueryParms;
	struct QueryDesc *queryDesc = (struct QueryDesc *) palloc0(sizeof(QueryDesc));

	_init_cdbdisp_buildPlanQueryParms(queryDesc);

	will_assign_value(serializeNode, uncompressed_size_out, 4096);
	will_assign_value(serializeNode, size, 3072);
	expect_any(serializeNode, node);
	expect_any(serializeNode, size);
	expect_any(serializeNode, uncompressed_size_out);

	char *splan = "serialized plan";
	will_return(serializeNode, splan);

	queryDesc->params = NULL;

	will_assign_value(serializeNode, size, 1024);
	expect_any(serializeNode, node);
	expect_any(serializeNode, size);
	expect_any(serializeNode, uncompressed_size_out);

	char *sddesc = "serialized dispatch desc";
	will_return(serializeNode, sddesc);

	CdbComponentDatabases *cdb_component_dbs = palloc(sizeof(*cdb_component_dbs));
	cdb_component_dbs->entry_db_info = palloc0(sizeof(CdbComponentDatabaseInfo));
	(cdb_component_dbs->entry_db_info[0]).hostip = "127.0.0.1";
	will_return(getComponentDatabases, cdb_component_dbs);

	seqServerCtl = palloc(sizeof(*seqServerCtl));
	seqServerCtl->seqServerPort = 1234;

	queryDesc->sourceText = "SELECT * FROM foo";
	queryDesc->extended_query = false;

	char *serializedDtxCtxInfo = "serialized dtx context info";
	expect_any(qdSerializeDtxContextInfo, size);
	expect_value(qdSerializeDtxContextInfo, wantSnapshot, true);
	expect_value(qdSerializeDtxContextInfo, inCursor, false);
	expect_any(qdSerializeDtxContextInfo, txnOptions);
	expect_string(qdSerializeDtxContextInfo, debugCaller, "cdbdisp_buildPlanQueryParms");
	will_assign_value(qdSerializeDtxContextInfo, size, 512);
	will_return(qdSerializeDtxContextInfo, serializedDtxCtxInfo);

	pQueryParms = cdbdisp_buildPlanQueryParms(queryDesc, false);

	assert_true(verify_built_query_parms(pQueryParms));
}

void
test__cdbdisp_buildPlanQueryParms__params(void **state)
{
	DispatchCommandQueryParms *pQueryParms;
	struct QueryDesc *queryDesc = (struct QueryDesc *) palloc0(sizeof(QueryDesc));

	_init_cdbdisp_buildPlanQueryParms(queryDesc);

	will_assign_value(serializeNode, uncompressed_size_out, 4096);
	will_assign_value(serializeNode, size, 3072);
	expect_any(serializeNode, node);
	expect_any(serializeNode, size);
	expect_any(serializeNode, uncompressed_size_out);

	char *splan = "serialized plan";
	will_return(serializeNode, splan);

	queryDesc->params = palloc0(sizeof(ParamListInfoData) + 4 * sizeof(ParamExternData));
	queryDesc->params->numParams = 4;
	queryDesc->params->params[0].ptype = InvalidOid;
	queryDesc->params->params[1].ptype = BOOLOID;
	queryDesc->params->params[1].value = 1;
	queryDesc->params->params[2].ptype = NAMEOID;
	queryDesc->params->params[2].isnull = true;
	queryDesc->params->params[3].ptype = NAMEOID;
	queryDesc->params->params[3].value = 12345678;

	expect_value(get_typlenbyval, typid, BOOLOID);
	expect_value_count(get_typlenbyval, typid, NAMEOID, 2);
	expect_any(get_typlenbyval, typlen);
	expect_any(get_typlenbyval, typlen);
	expect_any(get_typlenbyval, typlen);
	expect_any(get_typlenbyval, typbyval);
	expect_any(get_typlenbyval, typbyval);
	expect_any(get_typlenbyval, typbyval);
	will_assign_value(get_typlenbyval, typlen, (int16) 1);
	will_assign_value(get_typlenbyval, typlen, (int16 )64);
	will_assign_value(get_typlenbyval, typlen, (int16) 64);
	will_assign_value(get_typlenbyval, typbyval, (bool) true);
	will_assign_value(get_typlenbyval, typbyval, (bool) false);
	will_assign_value(get_typlenbyval, typbyval, (bool) false);
	will_be_called(get_typlenbyval);
	will_be_called(get_typlenbyval);
	will_be_called(get_typlenbyval);

	expect_any(serializeNode, node);
	expect_any(serializeNode, size);
	expect_any(serializeNode, uncompressed_size_out);

	char *sddesc = "serialized dispatch desc";
	will_return(serializeNode, sddesc);

	CdbComponentDatabases *cdb_component_dbs = palloc(sizeof(*cdb_component_dbs));
	cdb_component_dbs->entry_db_info = palloc0(sizeof(CdbComponentDatabaseInfo));
	(cdb_component_dbs->entry_db_info[0]).hostip = "127.0.0.1";
	will_return(getComponentDatabases, cdb_component_dbs);

	seqServerCtl = palloc(sizeof(*seqServerCtl));
	seqServerCtl->seqServerPort = 1234;

	char *serializedDtxCtxInfo = "serialized dtx context info";
	expect_any(qdSerializeDtxContextInfo, size);
	expect_any(qdSerializeDtxContextInfo, wantSnapshot);
	expect_any(qdSerializeDtxContextInfo, inCursor);
	expect_any(qdSerializeDtxContextInfo, txnOptions);
	expect_any(qdSerializeDtxContextInfo, debugCaller);
	will_return(qdSerializeDtxContextInfo, serializedDtxCtxInfo);

	pQueryParms = cdbdisp_buildPlanQueryParms(queryDesc, false);

	assert_true(pQueryParms->serializedParamslen ==
				140 /* 72(queryDesc->params) + sizeof(iparam) + 64(typlen of name) */);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] =
	{
		unit_test(test__cdbdisp_buildPlanQueryParms__Overflow_plan_size_in_kb),
		unit_test(test__cdbdisp_buildPlanQueryParms__no_params),
		unit_test(test__cdbdisp_buildPlanQueryParms__params)
	};

	/* There are assertions in dispatch code for this */
	Gp_role = GP_ROLE_DISPATCH;
	MemoryContextInit();

	return run_tests(tests);
}

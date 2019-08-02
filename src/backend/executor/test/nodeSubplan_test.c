#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../nodeSubplan.c"

/*
 * These functions are defined in explain_gp.c, which in turn is included
 * from explain.c. Since the mocker.py utility has trouble mocking this
 * structure, we include the relevant functions we need here instead.
 */
void cdbexplain_localExecStats(struct PlanState *planstate,
						  struct CdbExplain_ShowStatCtx *showstatctx)
{
	check_expected(planstate);
	check_expected(showstatctx);
	mock();
}

void
cdbexplain_recvExecStats(struct PlanState *planstate,
						 struct CdbDispatchResults *dispatchResults,
						 int sliceIndex,
						 struct CdbExplain_ShowStatCtx *showstatctx)
{
	check_expected(planstate);
	check_expected(dispatchResults);
	check_expected(sliceIndex);
	check_expected(showstatctx);
	mock();
}

void
cdbexplain_sendExecStats(QueryDesc *queryDesc)
{
	mock();
}

static void
cdbdispatch_succeed(void *ptr)
{
	EState *estate = (EState*) ptr;
	estate->dispatcherState = (CdbDispatcherState *) palloc(sizeof(CdbDispatcherState));
}

/* Function passed to testing framework
 * in order to force SetupInterconnect to fail */ 
static void
setupinterconnect_fail(void *ptr)
{
	PG_RE_THROW();
}


/* Checks CdbCheckDispatchResult is called when queryDesc
 * is not null (when shouldDispatch is true).
 * This test falls in PG_CATCH when SetupInterconnect
 * does not allocate queryDesc->estate->interconnect_context.
 * The test is successful if the */
static void 
test__ExecSetParamPlan__Check_Dispatch_Results(void **state) 
{

	/* Set plan to explain. */
	SubPlanState *plan = makeNode(SubPlanState);
	plan->xprstate.expr = (Expr *) makeNode(SubPlanState);
	plan->planstate = (PlanState *) makeNode(SubPlanState);
	plan->planstate->instrument = (Instrumentation *)palloc(sizeof(Instrumentation));
	plan->planstate->instrument->need_timer = true;
	plan->planstate->instrument->need_cdb = true;
	plan->planstate->plan = (Plan *) makeNode(SubPlanState);
	
	EState *estate = CreateExecutorState();

	/*Assign mocked estate to plan.*/
	((PlanState *)(plan->planstate))->state= estate;

	/*Re-use estate mocked object. Needed as input parameter for
	tested function */
	ExprContext *econtext = GetPerTupleExprContext(estate);

	/*Set QueryDescriptor input parameter for tested function */
	PlannedStmt   *plannedstmt = (PlannedStmt *)palloc(sizeof(PlannedStmt));
	QueryDesc *queryDesc = (QueryDesc *)palloc(sizeof(QueryDesc));
	queryDesc->plannedstmt = plannedstmt;
	queryDesc->estate = CreateExecutorState();
	queryDesc->estate->es_sliceTable = (SliceTable *) palloc(sizeof(SliceTable));
	
	Gp_role = GP_ROLE_DISPATCH;
	((SubPlan*)(plan->xprstate.expr))->initPlanParallel = true;

	will_be_called(isCurrentDtxTwoPhase);

	expect_any(CdbDispatchPlan,queryDesc);
	expect_any(CdbDispatchPlan,planRequiresTxn);
	expect_any(CdbDispatchPlan,cancelOnError);
	will_be_called_with_sideeffect(CdbDispatchPlan, &cdbdispatch_succeed, queryDesc->estate);

	expect_any(SetupInterconnect,estate);
	/* Force SetupInterconnect to fail */
	will_be_called_with_sideeffect(SetupInterconnect, &setupinterconnect_fail, NULL);

	expect_any(cdbexplain_localExecStats,planstate);
	expect_any(cdbexplain_localExecStats,showstatctx);
	will_be_called(cdbexplain_localExecStats);

	expect_any(cdbdisp_cancelDispatch,ds);
	will_be_called(cdbdisp_cancelDispatch);

	expect_any(CdbDispatchHandleError, ds);
	will_be_called(CdbDispatchHandleError);

	expect_any(cdbexplain_recvExecStats,planstate);
	expect_any(cdbexplain_recvExecStats,dispatchResults);
	expect_any(cdbexplain_recvExecStats,sliceIndex);
	expect_any(cdbexplain_recvExecStats,showstatctx);
	will_be_called(cdbexplain_recvExecStats);

	/* Catch PG_RE_THROW(); after cleaning with CdbCheckDispatchResult */
	PG_TRY();
		ExecSetParamPlan(plan,econtext,queryDesc);
	PG_CATCH();
		assert_true(true);
	PG_END_TRY();
}

int 
main(int argc, char* argv[]) 
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__ExecSetParamPlan__Check_Dispatch_Results)
	};

	MemoryContextInit();

	return run_tests(tests);
}

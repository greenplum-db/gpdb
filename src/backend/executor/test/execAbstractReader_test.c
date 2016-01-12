#include "../execAbstractReader.c"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"

#include "c.h"
#include "postgres.h"
#include "nodes/nodes.h"


/*
 * Checks if CreateOperatorReader creates an appropriate reader
 */
void
test__CreateOperatorReader__CreatesAppropriateReader(void **state)
{
	PlanState *planState = 0x1234;
	TupleTableSlot *outputSlot = NULL;
	OperatorTupleReader *opReader = CreateOperatorTupleReader(planState, &outputSlot);

	assert_true(IsA(opReader, OperatorTupleReader));
	assert_true(opReader->sourcePlan == planState);
	assert_true(opReader->baseReader.ReadNextTuple == OperatorTupleReader_ReadNext);
	assert_true(opReader->baseReader.PrepareForReScan == OperatorTupleReader_ReScan);
	assert_true(opReader->baseReader.EndReader == OperatorTupleReader_End);
}

/*
 * Checks if ReadNextTuple works
 */
void
test__ReadNextFromOperator__ReadsCorrectTuples(void **state)
{
	PlanState *planState = 0x1234;

	TupleTableSlot *outputSlot = NULL;
	OperatorTupleReader *opReader = CreateOperatorTupleReader(planState, &outputSlot);

	static int fakeOutput[] = {1, 2, 3, 0, 0};
	static int fakeOutputCount = sizeof(fakeOutput)/ sizeof(fakeOutput[0]);

	for (int i = 0; i < fakeOutputCount; i++)
	{
		will_return(ExecProcNode, fakeOutput[i]);
		expect_value(ExecProcNode, node, 0x1234);
	}

	for (int i = 0; i < fakeOutputCount; i++)
	{
		TupleTableSlot *outTuple = opReader->baseReader.ReadNextTuple(opReader);

		assert_true(outTuple == fakeOutput[i]);
	}
}

/*
 * Checks if ReadNextTuple works
 */
void
test__ReScanPlanState__ReScanChild(void **state)
{
	PlanState *planState = 0x1234;
	ExprContext *exprCtxt = 0x4567;

	TupleTableSlot *outputSlot = NULL;
	OperatorTupleReader *opReader = CreateOperatorTupleReader(planState, &outputSlot);

	will_be_called(ExecReScan);
	expect_value(ExecReScan, node, 0x1234);
	expect_value(ExecReScan, exprCtxt, 0x4567);

	OperatorTupleReader_ReScan(opReader, exprCtxt);
}

int
main(int argc, char* argv[])
{
        cmockery_parse_arguments(argc, argv);

        const UnitTest tests[] = {
            	unit_test(test__CreateOperatorReader__CreatesAppropriateReader),
            	unit_test(test__ReadNextFromOperator__ReadsCorrectTuples),
            	unit_test(test__ReScanPlanState__ReScanChild),
        };
        return run_tests(tests);
}

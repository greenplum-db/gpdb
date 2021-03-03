/*-------------------------------------------------------------------------
 *
 * gppc_test.c
 *	  libgppc test program
 *
 * Portions Copyright (c) 2012, Greenplum Inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/interfaces/gppc/test/gppc_test/gppc_test.c
 *
 *-------------------------------------------------------------------------
 */
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include "gppc.h"

static void
allocate_a_lot(GppcMemoryContext context)
{
	void   *ptr;
	int		i;
	GppcMemoryContext oldcontext;

	oldcontext = GppcMemoryContextSwitchTo(context);

	for (i = 0; i < 20; i++)
	{
		/*
		 * 900M bytes, completely depending on the default setting,
		 * but it's fairly reasonable, I believe...
		 */
		ptr = (int *) GppcAlloc(900 * 1000 * 1000);

		GppcMemoryContextReset(context);
	}

	GppcMemoryContextSwitchTo(oldcontext);
}

GPPC_FUNCTION_INFO(test_reset_current_memory);
GppcDatum test_reset_current_memory(GPPC_FUNCTION_ARGS);

GppcDatum
test_reset_current_memory(GPPC_FUNCTION_ARGS)
{
	allocate_a_lot(GppcGetCurrentMemoryContext());

	GPPC_RETURN_BOOL(true);
}

GPPC_FUNCTION_INFO(test_reset_child_memory);
GppcDatum test_reset_child_memory(GPPC_FUNCTION_ARGS);

GppcDatum
test_reset_child_memory(GPPC_FUNCTION_ARGS)
{
	GppcMemoryContext current, child;
	int	   *my_ptr;

	current = GppcGetCurrentMemoryContext();
	child = GppcMemoryContextCreate(current);

	my_ptr = (int *) GppcAlloc(10 * sizeof(int));
	my_ptr[0] = 42;

	allocate_a_lot(child);

	/* Should still be valid. */
	GPPC_RETURN_BOOL(my_ptr[0] == 42);
}

GPPC_FUNCTION_INFO(test_interrupt);
GppcDatum test_interrupt(GPPC_FUNCTION_ARGS);

GppcDatum
test_interrupt(GPPC_FUNCTION_ARGS)
{
	GppcCheckForInterrupts();

	GppcReport(GPPC_ERROR, "GppcCheckForInterrupts did not catch error");

	GPPC_RETURN_NULL();
}

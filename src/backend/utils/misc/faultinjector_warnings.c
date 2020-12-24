/*
 * faultinjector_warnings.c
 *
 * Plugin system for collecting warning functions and processing warnings
 * at a later time.
 *
 * Portions Copyright (c) 2019-Present VMware, Inc. or its affiliates.
 *
 * IDENTIFICATION
 *	    src/backend/utils/misc/faultinjector_warnings.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "nodes/pg_list.h"


/*
 * Implements
 */
#include "utils/faultinjector.h"
#include "faultinjector_warnings.h"


static List *warnings_list;


void
warnings_init(void)
{
	warnings_list = NULL;
}


/*
 * Provide a function that inspects an entry and warns the user
 * of an problematic configuration.
 *
 * void some_warning_function(FaultInjectorEntry_s faultEntry)
 * {
 *     if (isProblematic(faultEntry))
 *         elog(WARNING, "this fault injection configuration might have problems");
 * }
 *
 * add_fault_injection_warning(some_warning_function);
 */
void
register_fault_injection_warning(fault_injection_warning_function warning)
{
	warnings_list = lappend(warnings_list, warning);
}


/*
 * Process all configured warnings for a given faultEntry.
 *
 */
void
emit_warnings(FaultInjectorEntry_s faultEntry)
{
	ListCell *list_cell = NULL;

	foreach(list_cell, warnings_list)
	{
		fault_injection_warning_function warning_function =
			(fault_injection_warning_function) lfirst(list_cell);
		warning_function(faultEntry);
	}
}

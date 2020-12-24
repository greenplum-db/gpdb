/*
 * faultinjector_warnings.h
 *
 * Functions for internal usage by the fault injector framework
 *
 * Plugin system for collecting warning functions and processing warnings
 * at a later time.
 *
 * Portions Copyright (c) 2019-Present VMware, Inc. or its affiliates.
 *
 * IDENTIFICATION
 *	    src/backend/utils/misc/faultinjector_warnings.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef FAULTINJECTOR_WARNINGS_H
#define FAULTINJECTOR_WARNINGS_H

#include "postgres.h"
#include "nodes/pg_list.h"
#include "utils/faultinjector.h"

extern void warnings_init(void);
extern void emit_warnings(FaultInjectorEntry_s faultEntry);

#endif // FAULTINJECTOR_WARNINGS_H

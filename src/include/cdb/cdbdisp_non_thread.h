/*-------------------------------------------------------------------------
 *
 * cdbdisp_non_thread.h
 * routines for non-threaded implementation of dispatching commands
 * to the qExec processes.
 *
 * Copyright (c) 2005-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBDISP_NON_THREAD_H
#define CDBDISP_NON_THREAD_H

#include "cdb/cdbdisp.h"

extern DispatcherInternalFuncs NonThreadedFuncs;

#endif

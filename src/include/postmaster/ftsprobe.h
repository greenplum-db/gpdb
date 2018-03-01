/*-------------------------------------------------------------------------
 *
 * ftsprobe.h
 *	  Interface for fault tolerance service Sender.
 *
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/postmaster/ftsprobe.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef FTSPROBE_H
#define FTSPROBE_H

#include "cdb/ml_ipc.h" /* gettime_elapsed_ms */

void initPollFds(int size);
void ftsConnect(fts_context *context);
FtsProbeState nextFailedState(FtsProbeState state);
#endif

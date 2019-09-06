/*
 * backoff.h
 *
 *  Created on: Oct 20, 2009
 *      Author: siva
 */

#ifndef BACKOFF_H_
#define BACKOFF_H_

#include "fmgr.h"

/* GUCs */
extern bool gp_enable_resqueue_priority;
extern int gp_resqueue_priority_local_interval;
extern int gp_resqueue_priority_sweeper_interval;
extern int gp_resqueue_priority_inactivity_timeout;
extern int gp_resqueue_priority_grouping_timeout;
extern double gp_resqueue_priority_cpucores_per_segment;
extern char* gp_resqueue_priority_default_value;

extern void BackoffBackendEntryInit(int sessionid, int commandcount, Oid queueId);
extern void BackoffBackendEntryExit(void);
extern void BackoffStateInit(void);
extern Datum gp_adjust_priority_int(PG_FUNCTION_ARGS);
extern Datum gp_adjust_priority_value(PG_FUNCTION_ARGS);
extern Datum gp_list_backend_priorities(PG_FUNCTION_ARGS);

extern void BackoffSweeperMain(Datum main_arg);
extern bool BackoffSweeperStartRule(Datum main_arg);


#endif /* BACKOFF_H_ */

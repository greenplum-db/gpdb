/*
 * backoff.h
 *
 *  Created on: Oct 20, 2009
 *      Author: siva
 */

#ifndef BACKOFF_H_
#define BACKOFF_H_

#include "postgres.h"
#include "storage/proc.h"

/* GUCs */
extern bool gp_enable_resqueue_priority;
extern int gp_resqueue_priority_local_interval;
extern int gp_resqueue_priority_sweeper_interval;
extern int gp_resqueue_priority_inactivity_timeout;
extern int gp_resqueue_priority_grouping_timeout;
extern double gp_resqueue_priority_cpucores_per_segment;
extern char* gp_resqueue_priority_default_value;

extern void BackoffBackendEntryInit(int sessionid, int commandcount, int weight);
extern void BackoffBackendEntryExit(void);
extern void BackoffBackendTick(void);
extern void BackoffStateInit(void);
extern Datum gp_adjust_priority_int(PG_FUNCTION_ARGS);
extern Datum gp_adjust_priority_value(PG_FUNCTION_ARGS);
extern Datum gp_list_backend_priorities(PG_FUNCTION_ARGS);

extern int backoff_start(void);

extern int BackoffSuperuserStatementWeight(void);
extern int ResourceQueueGetPriorityWeight(Oid queueId);
extern int BackoffDefaultWeight(void);

#endif /* BACKOFF_H_ */

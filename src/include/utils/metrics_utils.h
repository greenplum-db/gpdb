/*-------------------------------------------------------------------------
 *
 * metrics_utils.h
 *	  Definitions for query info collector enum and functions
 *
 * Portions Copyright (c) 2017-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/metrics_utils.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef METRICS_UTILS_H
#define METRICS_UTILS_H 

typedef enum
{	
	METRICS_PLAN_NODE_INITIALIZE = 100,
	METRICS_PLAN_NODE_EXECUTING,
	METRICS_PLAN_NODE_FINISHED,
	
	METRICS_QUERY_SUBMIT = 200,
	METRICS_QUERY_START,
	METRICS_QUERY_DONE,
	METRICS_QUERY_ERROR,
	METRICS_QUERY_CANCELING,
	METRICS_QUERY_CANCELED
} QueryMetricsStatus;

typedef void (*query_info_collect_hook_type)(QueryMetricsStatus, void *);
extern PGDLLIMPORT query_info_collect_hook_type query_info_collect_hook;

#endif   /* METRICS_UTILS_H */

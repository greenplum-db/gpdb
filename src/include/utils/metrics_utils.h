/*-------------------------------------------------------------------------
 *
 * metrics_utils.h
 *	  Definitions for query info collector enum and functions
 *
 * Portions Copyright (c) 2017-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/metrics_utils.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef METRICS_UTILS_H
#define METRICS_UTILS_H 

/* whether the query is a spi/function inner/top-level query or for extension usage */
#define	TOP_LEVEL_QUERY 	 (0U)
#define	SPI_INNER_QUERY 	 (1U)
#define	FUNCTION_INNER_QUERY 	 (2U)

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
	METRICS_QUERY_CANCELED,

	METRICS_INNER_QUERY_DONE = 300
} QueryMetricsStatus;

typedef void (*query_info_collect_hook_type)(QueryMetricsStatus, void *);
extern PGDLLIMPORT query_info_collect_hook_type query_info_collect_hook;

#endif   /* METRICS_UTILS_H */

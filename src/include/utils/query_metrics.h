/*-------------------------------------------------------------------------
 *
 * query_metrics.h
 *	  Definitions for query metrics struct and functions
 *
 * Portions Copyright (c) 2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/query_metrics.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef QUERY_METRICS_H
#define QUERY_METRICS_H

#include "postgres.h"
#include "gpmon/gpmon.h"
#include "executor/execdesc.h"
#include "nodes/execnodes.h"

typedef struct
{
	int			mcsock;
	pid_t		pid;
	struct sockaddr_in mcaddr;
} metrics_conn;

typedef enum MetricsNodeStatus
{
	METRICS_NODE_INITIALIZE = 0,
	METRICS_NODE_EXECUTING,
	METRICS_NODE_FINISHED
} MetricsNodeStatus;

typedef enum MetricsQueryStatus
{
	METRICS_QUERY_INVALID = 0,
	METRICS_QUERY_SUBMIT,
	METRICS_QUERY_START,
	METRICS_QUERY_DONE,
	METRICS_QUERY_ERROR,
	METRICS_QUERY_CANCELING
} MetricsQueryStatus;

#define METRICS_PACKET_VERSION   1

enum metrics_pkttype_t
{
	METRICS_PKTTYPE_NONE = 0,
	METRICS_PKTTYPE_NODE = 20,
	METRICS_PKTTYPE_INSTR = 21,
	METRICS_PKTTYPE_QUERY = 22
};

typedef struct metrics_query_id
{
	int32		tmid;			/* transaction time */
	int32		ssid;			/* session id */
	int32		ccnt;			/* command count */
} metrics_query_id;

typedef struct metrics_node_t
{
	metrics_query_id qid;
	int16		segid;			/* segment id */
	int16		nid;			/* node id */
	int32		pid;			/* process id */
	int32		pnid;			/* plan parent node id */
	int32		node_type;		/* node type */
	int32		plan_width;		/* plan_width from Plan */
	double		time;			/* timestamp of this event */
	double		startup_cost;	/* startup_cost from Plan */
	double		total_cost;		/* total_cost from Plan */
	double		plan_rows;		/* plan_rows from Plan */
	int16		status;			/* node status */
} metrics_node_t;

typedef struct metrics_query_t
{
	metrics_query_id qid;
	char		user[NAMEDATALEN];
	char		db[NAMEDATALEN];
	int32		tsubmit,
				tstart,
				tfin;
	int16		status;
	int16		command_type;	/* select|insert|update|delete */
	int16		plan_gen;		/* planner|orca */
} metrics_query_t;

typedef struct metrics_packet_t
{
	int16		version;
	int16		pkttype;
	union
	{
		metrics_query_t q;
		metrics_node_t node;
	}			u;
} metrics_packet_t;

/* GUCs */
extern int	gp_query_metrics_port;

/* Interface */
extern void metrics_init(void);
extern void metrics_send(metrics_packet_t *p);

extern void metrics_send_query_info(QueryDesc *qd, MetricsQueryStatus status);
extern void InitNodeMetricsInfoPkt(Plan *plan, QueryDesc *qd);
extern void UpdateNodeMetricsInfoPkt(PlanState *ps, MetricsNodeStatus status);
#endif   /* QUERY_METRICS_H */

/*-------------------------------------------------------------------------
 *
 * metrics.c
 *    Functions for sending query metrics packets
 *
 *  Created on: Oct 11, 2017
 *      Author: Hao Wang & Teng Zhang
 *
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/misc/metrics.c
 *
 *-------------------------------------------------------------------------
*/

#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "postgres.h"
#include "cdb/cdbvars.h"
#include "nodes/execnodes.h"
#include "portability/instr_time.h"
#include "utils/query_metrics.h"

metrics_conn conn = {-1};

void
metrics_init(void)
{
	pid_t		pid = getpid();
	int			sock;

	if (pid == conn.pid)
	{
		return;
	}
	close(conn.mcsock);
	conn.mcsock = -1;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1)
	{
		elog(WARNING, "metrics: cannot create socket (%m)");
	}
	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1)
	{
		elog(WARNING, "fcntl(F_SETFL, O_NONBLOCK) failed");
	}
	if (fcntl(sock, F_SETFD, 1) == -1)
	{
		elog(WARNING, "fcntl(F_SETFD) failed");
	}
	conn.mcsock = sock;
	memset(&conn.mcaddr, 0, sizeof(conn.mcaddr));
	conn.mcaddr.sin_family = AF_INET;
	conn.mcaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	conn.mcaddr.sin_port = htons(gp_query_metrics_port);
	conn.pid = pid;
}

void
metrics_send(metrics_packet_t *p)
{
	int			n;

	if (conn.mcsock >= 0)
	{
		n = sizeof(*p);
		if (n != sendto(conn.mcsock, (const char *) p, n, 0,
						(struct sockaddr *) &conn.mcaddr,
						sizeof(conn.mcaddr)))
		{
			elog(LOG, "metrics: cannot send (%m socket %d)", conn.mcsock);
		}
	}
}

/* Node info */
static void
MakeMetricsNodeInfo(metrics_packet_t *pkt, Plan *plan, gpmon_packet_t *gpmon_pkt, MetricsNodeStatus status)
{
	instr_time	curr;

	memset(pkt, 0x00, sizeof(metrics_packet_t));

	pkt->version = METRICS_PACKET_VERSION;
	pkt->pkttype = METRICS_PKTTYPE_NODE;

	if (gpmon_pkt && gpmon_pkt->pkttype == GPMON_PKTTYPE_QLOG)
	{
		/* Copy query identities from parent QLog */
		pkt->u.node.qid.tmid = gpmon_pkt->u.qlog.key.tmid;
		pkt->u.node.qid.ssid = gpmon_pkt->u.qlog.key.ssid;
		pkt->u.node.qid.ccnt = gpmon_pkt->u.qlog.key.ccnt;
	}
	else if (gpmon_pkt && gpmon_pkt->pkttype == GPMON_PKTTYPE_QEXEC)
	{
		/* Copy query identities from parent QExec */
		pkt->u.node.qid.tmid = gpmon_pkt->u.qexec.key.tmid;
		pkt->u.node.qid.ssid = gpmon_pkt->u.qexec.key.ssid;
		pkt->u.node.qid.ccnt = gpmon_pkt->u.qexec.key.ccnt;
	}
	else
	{
		gpmon_gettmid(&(pkt->u.node.qid.tmid));
		pkt->u.node.qid.ssid = gp_session_id;
		pkt->u.node.qid.ccnt = gp_command_count;
	}

	pkt->u.node.segid = Gp_segment;
	pkt->u.node.pid = MyProcPid;
	pkt->u.node.nid = plan->plan_node_id;
	pkt->u.node.status = status;
	pkt->u.node.node_type = plan->type;
	pkt->u.node.plan_width = plan->plan_width;
	pkt->u.node.pnid = plan->plan_parent_node_id;
	INSTR_TIME_SET_CURRENT(curr);
	pkt->u.node.time = INSTR_TIME_GET_DOUBLE(curr);
	pkt->u.node.startup_cost = plan->startup_cost;
	pkt->u.node.total_cost = plan->total_cost;
	pkt->u.node.plan_rows = plan->plan_rows;
}

static void
SendPlanNodeMetricsPkt(Plan *plan, gpmon_packet_t *gpmon_pkt, MetricsNodeStatus status)
{
	metrics_packet_t pkt;

	if (!plan)
		return;

	if (!gp_enable_query_metrics)
		return;

	MakeMetricsNodeInfo(&pkt, plan, gpmon_pkt, status);
	metrics_send(&pkt);
}

void
InitNodeMetricsInfoPkt(Plan *plan, QueryDesc *qd)
{
	SendPlanNodeMetricsPkt(plan, qd->gpmon_pkt, METRICS_NODE_INITIALIZE);
}

void
UpdateNodeMetricsInfoPkt(PlanState *ps, MetricsNodeStatus status)
{
	if (!ps || !ps->state || LocallyExecutingSliceIndex(ps->state) != currentSliceId)
		return;

	SendPlanNodeMetricsPkt(ps->plan, &(ps->gpmon_pkt), status);
}

/* Query info */
void
metrics_send_query_info(QueryDesc *qd, MetricsQueryStatus status)
{
	metrics_packet_t pkt;
	gpmon_qlog_t *qlog;

	if (!qd)
		return;

	if (!gp_enable_query_metrics)
		return;

	if (conn.mcsock < 0)
		return;

	Assert(qd->gpmon_pkt);
	Assert(qd->gpmon_pkt->pkttype == GPMON_PKTTYPE_QLOG);

	qlog = &(qd->gpmon_pkt->u.qlog);

	memset(&pkt, 0x00, sizeof(metrics_packet_t));
	pkt.version = METRICS_PACKET_VERSION;
	pkt.pkttype = METRICS_PKTTYPE_QUERY;
	pkt.u.q.qid.tmid = qlog->key.tmid;
	pkt.u.q.qid.ssid = qlog->key.ssid;
	pkt.u.q.qid.ccnt = qlog->key.ccnt;
	memcpy(pkt.u.q.db, qlog->db, mul_size(sizeof(char), NAMEDATALEN));
	memcpy(pkt.u.q.user, qlog->user, mul_size(sizeof(char), NAMEDATALEN));
	pkt.u.q.tsubmit = qlog->tsubmit;
	pkt.u.q.tstart = qlog->tstart;
	pkt.u.q.tfin = qlog->tfin;
	pkt.u.q.command_type = qd->operation;
	if (status <= METRICS_QUERY_START && qd->operation != CMD_UTILITY && qd->plannedstmt)
	{
		pkt.u.q.plan_gen = qd->plannedstmt->planGen;
	}
	pkt.u.q.status = status;

	metrics_send(&pkt);
}

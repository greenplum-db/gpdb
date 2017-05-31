/*
 * execGpmon.c
 *   Gpmon related functions inside executor.
 *
 * Copyright (c) 2012 - present, EMC/Greenplum
 */
#include "postgres.h"

#include "executor/executor.h"
#include "utils/lsyscache.h"
#include "cdb/cdbvars.h"
#include "miscadmin.h"

char * GetScanRelNameGpmon(Oid relid, char schema_rel_name[SCAN_REL_NAME_BUF_SIZE])
{
	if (relid > 0)
	{
		char *relname = get_rel_name(relid);
		char *schemaname = get_namespace_name(get_rel_namespace(relid));
		snprintf(schema_rel_name, SCAN_REL_NAME_BUF_SIZE, "%s.%s", schemaname, relname);
		if (relname)
		{
			pfree(relname);
		}
		
		if (schemaname)
		{
			pfree(schemaname);
		}
	}
	return schema_rel_name;
}

void CheckSendPlanStateGpmonPkt(PlanState *ps)
{
	if(!ps)
		return;

	if(gp_enable_gpperfmon)
	{
		if(!ps->fHadSentGpmon || ps->gpmon_plan_tick != gpmon_tick)
		{
			if(ps->state && LocallyExecutingSliceIndex(ps->state) == currentSliceId)
			{
				gpmon_send(&ps->gpmon_pkt);
			}
			
			ps->fHadSentGpmon = true;
		}

		ps->gpmon_plan_tick = gpmon_tick;
	}
}

void EndPlanStateGpmonPkt(PlanState *ps)
{
	if(!ps)
		return;

	ps->gpmon_pkt.u.qexec.status = (uint8)PMNS_Finished;

	if(gp_enable_gpperfmon &&
	   ps->state &&
	   LocallyExecutingSliceIndex(ps->state) == currentSliceId)
	{
		gpmon_send(&ps->gpmon_pkt);
	}
}

/*
 * InitPlanNodeGpmonPkt -- initialize the init gpmon package, and send it off.
 */
void InitPlanNodeGpmonPkt(Plan *plan, gpmon_packet_t *gpmon_pkt, EState *estate)
{
	if(!plan)
		return;

	memset(gpmon_pkt, 0, sizeof(gpmon_packet_t));

	gpmon_pkt->magic = GPMON_MAGIC;
	gpmon_pkt->version = GPMON_PACKET_VERSION;
	gpmon_pkt->pkttype = GPMON_PKTTYPE_QEXEC;

	gpmon_gettmid(&gpmon_pkt->u.qexec.key.tmid);
	gpmon_pkt->u.qexec.key.ssid = gp_session_id;
	gpmon_pkt->u.qexec.key.ccnt = gp_command_count;
	gpmon_pkt->u.qexec.key.hash_key.segid = Gp_segment;
	gpmon_pkt->u.qexec.key.hash_key.pid = MyProcPid;
	gpmon_pkt->u.qexec.key.hash_key.nid = plan->plan_node_id;

	gpmon_pkt->u.qexec.pnid = plan->plan_parent_node_id;

	gpmon_pkt->u.qexec.rowsout = 0;

	gpmon_pkt->u.qexec.status = (uint8)PMNS_Initialize;

	if(gp_enable_gpperfmon && estate)
	{
		gpmon_send(gpmon_pkt);
	}

	gpmon_pkt->u.qexec.status = (uint8)PMNS_Executing;
}

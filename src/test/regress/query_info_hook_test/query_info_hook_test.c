#include "postgres.h"

#include "fmgr.h"
#include "cdb/cdbvars.h"
#include "utils/metrics_utils.h"
#include "nodes/execnodes.h"
#include "nodes/print.h"
#include "executor/execdesc.h"

PG_MODULE_MAGIC;

void _PG_init(void);
void _PG_fini(void);

static query_info_collect_hook_type prev_query_info_collect_hook;

static void
test_hook(QueryMetricsStatus, void* args);

void
_PG_init(void)
{
	prev_query_info_collect_hook = query_info_collect_hook;
	query_info_collect_hook = test_hook;
}

void
_PG_fini(void)
{
	query_info_collect_hook = prev_query_info_collect_hook;
}

static void
test_hook(QueryMetricsStatus status, void* args)
{
	if (Gp_role != GP_ROLE_DISPATCH)
		return;
	if (GpIdentity.segindex > -1)
		return;

	switch (status)
	{
		case METRICS_PLAN_NODE_INITIALIZE:
			switch (((QueryDesc *)args)->plannedstmt->metricsQueryType)
			{
				case TOP_LEVEL_QUERY: 
					ereport(WARNING, (errmsg("Plan node initializing")));
					break;
				case SPI_INNER_QUERY:
					ereport(WARNING, (errmsg("Plan node of SPI inner query initializing")));
					break;
				case FUNCTION_INNER_QUERY:
					ereport(WARNING, (errmsg("Plan node of function inner query initializing")));
					break;
			}
			break;
		case METRICS_PLAN_NODE_EXECUTING:
			switch (((PlanState *)args)->state->es_plannedstmt->metricsQueryType)
			{
				case TOP_LEVEL_QUERY: 
					ereport(WARNING, (errmsg("Plan node executing node_type: %s", 
												plannode_type(((PlanState *)args)->plan))));
					break;
				case SPI_INNER_QUERY:
					ereport(WARNING, (errmsg("Plan node of SPI inner query executing node_type: %s", 
												plannode_type(((PlanState *)args)->plan))));
					break;
				case FUNCTION_INNER_QUERY:
					ereport(WARNING, (errmsg("Plan node of function inner query executing node_type: %s", 
											plannode_type(((PlanState *)args)->plan))));
					break;
			}
			break;
		case METRICS_PLAN_NODE_FINISHED:
			switch (((PlanState *)args)->state->es_plannedstmt->metricsQueryType)
			{
				case TOP_LEVEL_QUERY: 
					ereport(WARNING, (errmsg("Plan node finished")));
					break;
				case SPI_INNER_QUERY:
					ereport(WARNING, (errmsg("Plan node of SPI inner query finished")));
					break;
				case FUNCTION_INNER_QUERY:
					ereport(WARNING, (errmsg("Plan node of function inner query finished")));
					break;
			}
			break;
		case METRICS_QUERY_SUBMIT:
			switch (((QueryDesc *)args)->plannedstmt->metricsQueryType)
			{
				case TOP_LEVEL_QUERY: 
					ereport(WARNING, (errmsg("Query submit")));
					break;
				case SPI_INNER_QUERY:
					ereport(WARNING, (errmsg("SPI inner query submit")));
					break;
				case FUNCTION_INNER_QUERY:
					ereport(WARNING, (errmsg("function inner query submit")));
					break;
			}
			break;
		case METRICS_QUERY_START:
			switch (((QueryDesc *)args)->plannedstmt->metricsQueryType)
			{
				case TOP_LEVEL_QUERY: 
					ereport(WARNING, (errmsg("Query start")));
					break;
				case SPI_INNER_QUERY:
					ereport(WARNING, (errmsg("SPI inner query start")));
					break;
				case FUNCTION_INNER_QUERY:
					ereport(WARNING, (errmsg("function inner query start")));
					break;
			}
			break;
		case METRICS_QUERY_DONE:
			ereport(WARNING, (errmsg("Query done")));
			break;
		case METRICS_INNER_QUERY_DONE:
			ereport(WARNING, (errmsg("Inner query done")));
			break;
		case METRICS_QUERY_ERROR:
			switch (((QueryDesc *)args)->plannedstmt->metricsQueryType)
			{
				case TOP_LEVEL_QUERY: 
					ereport(WARNING, (errmsg("Query Error")));
					break;
				case SPI_INNER_QUERY:
					ereport(WARNING, (errmsg("SPI inner query Error")));
					break;
				case FUNCTION_INNER_QUERY:
					ereport(WARNING, (errmsg("function inner query Error")));
					break;
			}
			break;
		case METRICS_QUERY_CANCELING:
			switch (((QueryDesc *)args)->plannedstmt->metricsQueryType)
			{
				case TOP_LEVEL_QUERY: 
					ereport(WARNING, (errmsg("Query Canceling")));
					break;
				case SPI_INNER_QUERY:
					ereport(WARNING, (errmsg("SPI inner query Canceling")));
					break;
				case FUNCTION_INNER_QUERY:
					ereport(WARNING, (errmsg("function inner query Canceling")));
					break;
			}
			break;
		case METRICS_QUERY_CANCELED:
			switch (((QueryDesc *)args)->plannedstmt->metricsQueryType)
			{
				case TOP_LEVEL_QUERY: 
					ereport(WARNING, (errmsg("Query Canceled")));
					break;
				case SPI_INNER_QUERY:
					ereport(WARNING, (errmsg("SPI inner query Canceled")));
					break;
				case FUNCTION_INNER_QUERY:
					ereport(WARNING, (errmsg("function inner query Canceled")));
					break;
			}
			break;
	}
}



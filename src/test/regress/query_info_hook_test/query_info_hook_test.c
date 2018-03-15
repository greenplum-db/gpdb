#include "postgres.h"

#include "fmgr.h"
#include "utils/metrics_utils.h"

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
	switch (status)
	{
		case METRICS_PLAN_NODE_INITIALIZE:
			ereport(WARNING, (errmsg("Plan node initializing")));
			break;
		case METRICS_PLAN_NODE_EXECUTING:
			ereport(WARNING, (errmsg("Plan node executing")));
			break;
		case METRICS_PLAN_NODE_FINISHED:
			ereport(WARNING, (errmsg("Plan node finished")));
			break;
		case METRICS_QUERY_SUBMIT:
			ereport(WARNING, (errmsg("Query submit")));
			break;
		case METRICS_QUERY_START:
			ereport(WARNING, (errmsg("Query start")));
			break;
		case METRICS_QUERY_DONE:
			ereport(WARNING, (errmsg("Query Done")));
			break;
		case METRICS_QUERY_ERROR:
			ereport(WARNING, (errmsg("Query Error")));
			break;
		case METRICS_QUERY_CANCELING:
			ereport(WARNING, (errmsg("Query Canceling")));
			break;
		case METRICS_QUERY_CANCELED:
			ereport(WARNING, (errmsg("Query Canceled")));
			break;
	}
}


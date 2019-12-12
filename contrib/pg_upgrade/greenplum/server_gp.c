#include "greenplum_cluster_info.h"
#include "greenplum_cluster_info_internal.h"
#include "pg_upgrade_greenplum.h"

char *
greenplum_extra_pg_ctl_flags(GreenplumClusterInfo *info)
{
	int gp_dbid;
	int gp_content_id;

	if (is_greenplum_dispatcher_mode())
	{
		gp_dbid       = 1;
		gp_content_id = -1;
	}
	else
	{
		gp_dbid       = get_gp_dbid(info);
		gp_content_id = 0;
	}

	return psprintf("--gp_dbid=%d --gp_contentid=%d ", gp_dbid, gp_content_id);
}

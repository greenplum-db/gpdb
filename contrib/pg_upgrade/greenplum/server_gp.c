#include "extra_cluster_info.h"
#include "pg_upgrade.h"
#include "pg_upgrade_greenplum.h"

char *
get_extra_pg_ctl_flags(ExtraClusterInfo *extraClusterInfo)
{
	int gp_dbid;
	int gp_content_id = 0;
	GreenplumClusterInfo *greenplumClusterInfo;

	greenplumClusterInfo = (GreenplumClusterInfo *) extraClusterInfo;
	gp_dbid = greenplumClusterInfo->gp_dbid;

	if (user_opts.segment_mode == DISPATCHER)
		gp_content_id = -1;

	return psprintf(" --gp_dbid=%d --gp_contentid=%d ",
	                gp_dbid,
	                gp_content_id);
}

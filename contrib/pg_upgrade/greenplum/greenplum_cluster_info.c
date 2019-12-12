#include "postgres_fe.h"
#include "greenplum_cluster_info_internal.h"

static int gp_dbid_not_set = -1;

struct GreenplumClusterInfoData
{
	int gp_dbid;
};

GreenplumClusterInfo * make_cluster_info(void)
{
	GreenplumClusterInfo *info = palloc0(sizeof(GreenplumClusterInfo));
	info->gp_dbid = gp_dbid_not_set;
	return info;
}

int
get_gp_dbid(GreenplumClusterInfo *info)
{
	return info->gp_dbid;
}

void
set_gp_dbid(GreenplumClusterInfo *info, int gp_dbid)
{
	info->gp_dbid = gp_dbid;
}

bool
is_gp_dbid_set(GreenplumClusterInfo *info)
{
	return info->gp_dbid != gp_dbid_not_set;
}

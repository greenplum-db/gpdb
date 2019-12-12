#include "postgres_fe.h"
#include "greenplum_cluster_info.h"

GreenplumClusterInfo *make_cluster_info(void);

int get_gp_dbid(GreenplumClusterInfo *info);

void set_gp_dbid(GreenplumClusterInfo *info, int gp_dbid);

bool is_gp_dbid_set(GreenplumClusterInfo *info);

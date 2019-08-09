#include "pg_upgrade.h"

/*
 * GPDB: clusters need to connect in utility mode during upgrade
 *
 */
void
init_cluster(ClusterInfo *cluster)
{
	cluster->use_utility_mode = true;
}

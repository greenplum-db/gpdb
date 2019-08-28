#include "pg_upgrade.h"


#include "check_greenplum_internal.h"


static void
perform_check(check_function check, ClusterInfo *cluster, Queries *queries)
{
	if (check(cluster, queries))
	{
		check_ok();
		return;
	}

	check_failed();
}


void
perform_greenplum_checks(
	check_function gp_checks_list[],
	int gp_checks_list_length,
	ClusterInfo *cluster,
	Queries *queries)
{
	size_t i;

	for (i = 0; i < gp_checks_list_length; i++)
	{
		perform_check(gp_checks_list[i], cluster, queries);
	}
}

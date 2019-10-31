#include <stdlib.h>

#include "gpdb6-cluster.h"

void
startGpdbSixCluster(void)
{
	system(""
		   ". ./gpdb6/greenplum_path.sh; "
		   ". ./configuration/gpdb6-env.sh; "
		   "export MASTER_DATA_DIRECTORY=./gpdb6-data/qddir/demoDataDir-1; "
		   "./gpdb6/bin/gpstart -a --skip_standby_check --no_standby"
		);
}

void
stopGpdbSixCluster(void)
{
	system(""
		   ". ./gpdb6/greenplum_path.sh; \n"
		   ". ./configuration/gpdb6-env.sh; \n"
		   "export MASTER_DATA_DIRECTORY=./gpdb6-data/qddir/demoDataDir-1; \n"
		   "./gpdb6/bin/gpstop -af"
		);
}

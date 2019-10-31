#include <stdlib.h>

#include "gpdb5-cluster.h"

void
startGpdbFiveCluster(void)
{
	system(""
		   ". $PWD/gpdb5/greenplum_path.sh; "
		   "export PGPORT=50000; "
		   "export MASTER_DATA_DIRECTORY=$PWD/gpdb5-data/qddir/demoDataDir-1; "
		   "$PWD/gpdb5/bin/gpstart -a --skip_standby_check --no_standby"
		);
}

void
stopGpdbFiveCluster(void)
{
	system(""
		   ". $PWD/gpdb5/greenplum_path.sh; \n"
		   "export PGPORT=50000; \n"
		   "export MASTER_DATA_DIRECTORY=$PWD/gpdb5-data/qddir/demoDataDir-1; \n"
		   "$PWD/gpdb5/bin/gpstop -af"
		);
}


#include "gpdb5-cluster.h"
#include "stdlib.h"

void
startGpdbFiveCluster(void)
{
	system(""
	       "source $PWD/gpdb5/greenplum_path.sh; "
	       "PGPORT=50000; "
	       "MASTER_DATA_DIRECTORY=$PWD/gpdb5-data/qddir/demoDataDir-1; "
	       "$PWD/gpdb5/bin/gpstart -a"
	);
}

void
stopGpdbFiveCluster(void)
{
	system(""
	       "source $PWD/gpdb5/greenplum_path.sh; \n"
	       "PGPORT=50000; \n"
	       "MASTER_DATA_DIRECTORY=$PWD/gpdb5-data/qddir/demoDataDir-1; \n"
	       "$PWD/gpdb5/bin/gpstop -a"
	);
}


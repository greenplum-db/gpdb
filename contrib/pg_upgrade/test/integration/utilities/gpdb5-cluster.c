#include "gpdb5-cluster.h"
#include "stdlib.h"

void
startGpdbFiveCluster(void)
{
	system(""
	       "source ./gpdb5/greenplum_path.sh; "
	       "PGPORT=50000; "
	       "MASTER_DATA_DIRECTORY=./gpdb5-data-copy/qddir/demoDataDir-1; "
	       "gpstart -a"
	);
}

void
stopGpdbFiveCluster(void)
{
	system(""
	       "source ./gpdb5/greenplum_path.sh; \n"
	       "PGPORT=50000; \n"
	       "MASTER_DATA_DIRECTORY=./gpdb5-data-copy/qddir/demoDataDir-1; \n"
	       "gpstop -a"
	);
}


#include "stdlib.h"


#include "gpdb6-cluster.h"


void
startGpdbSixCluster(void)
{
	system(""
	       "source ./gpdb6/greenplum_path.sh; "
	       "PGPORT=60000; "
	       "MASTER_DATA_DIRECTORY=./gpdb6-data-copy/qddir/demoDataDir-1; "
	       "gpstart -a"
	);
}

void
stopGpdbSixCluster(void)
{
	system(""
	       "source ./gpdb6/greenplum_path.sh; \n"
	       "PGPORT=60000; \n"
	       "MASTER_DATA_DIRECTORY=./gpdb6-data-copy/qddir/demoDataDir-1; \n"
	       "gpstop -a"
	);
}


#include "stdlib.h"

#include "gpdb6-cluster.h"

void
startGpdbSixCluster(void)
{
	system(""
		   ". ./gpdb6/greenplum_path.sh; "
		   "export PGPORT=60000; "
		   "export MASTER_DATA_DIRECTORY=./gpdb6-data/qddir/demoDataDir-1; "
		   "./gpdb6/bin/gpstart -a"
		);
}

void
stopGpdbSixCluster(void)
{
	system(""
		   ". ./gpdb6/greenplum_path.sh; \n"
		   "export PGPORT=60000; \n"
		   "export MASTER_DATA_DIRECTORY=./gpdb6-data/qddir/demoDataDir-1; \n"
		   "./gpdb6/bin/gpstop -a"
		);
}

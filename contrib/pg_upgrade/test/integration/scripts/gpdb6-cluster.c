#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#include "../utilities/gpdb6-cluster.h"

int
main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("\nusage: ./scripts/gpdb6-cluster [start|stop]\n");
		exit(1);
	}

	char *const command = argv[1];

	if (strncmp(command, "start", 5)) {
		startGpdbSixCluster();
	}
	
	if (strncmp(command, "stop", 4)) {
		stopGpdbSixCluster();
	}

}
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "utilities/gpdb5-cluster.h"

int
main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("number of arguments: %d", argc);
		printf("\nusage: ./scripts/gpdb5-cluster [start|stop]\n");
		exit(1);
	}

	char	   *const command = argv[1];

	if (strncmp(command, "start", 5) == 0)
		startGpdbFiveCluster();

	if (strncmp(command, "stop", 4) == 0)
		stopGpdbFiveCluster();
}

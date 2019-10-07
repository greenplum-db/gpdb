#include <stdlib.h>
#include <stdio.h>

#include "gpdb5-cluster.h"
#include "gpdb6-cluster.h"

#include "query-helpers.h"
#include "test-helpers.h"

void
resetGpdbFiveDataDirectories(void)
{
	printf("\nMaking a copy of gpdb5 data directories.\n");
	system("rsync -a --delete ./gpdb5-data-copy/ ./gpdb5-data");
}

void
resetGpdbSixDataDirectories(void)
{
	printf("\nMaking a copy of gpdb6 data directories.\n");
	system("rsync -a --delete ./gpdb6-data-copy/ ./gpdb6-data");
}

PGconn *
connectToFive()
{
	return connectTo(GPDB_FIVE_PORT);
}

PGconn *
connectToSix()
{
	return connectTo(GPDB_SIX_PORT);
}

#include <stdlib.h>
#include <stdio.h>

#include "test-helpers.h"
#include "query-helpers.h"
#include "test-upgrade-helpers.h"
#include "pqexpbuffer.h"

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
	return connectToFiveOnDatabase("postgres");
}

PGconn *
connectToFiveOnDatabase(char *database_name)
{
	return connectTo(50000, database_name);
}

PGconn *
connectToSix()
{
	return connectToSixOnDatabase("postgres");
}

PGconn *
connectToSixOnDatabase(char *database_name)
{
	return connectTo(60000, database_name);
}
